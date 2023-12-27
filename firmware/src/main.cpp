/*
Copyright 2023, Pontus Rydin

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
#include <SPI.h>
#include <MCP3202.h>
#define USE_TIMER_3 true
#include <TimerInterrupt.h>
#include <ISR_Timer.h>
#include <MCP_DAC.h>
#include <RotaryEncoder.h>
#include "Display.hpp"
#include "TempControl.hpp"
#include "Average.hpp"
#include "ControlKnob.hpp"
#include "Calibration.hpp"

// Voltage dial pins
#define ROTARY_DT_1 11
#define ROTARY_CLK_1 10
#define ROTARY_SW_1 12

// Current dial pins
#define ROTARY_DT_2 22 // Fix for broken D7 on my Arduino.
#define ROTARY_CLK_2 5
#define ROTARY_SW_2 9

// ADC/DAC pins
#define DAC_CS 13 // DAC chip select
#define ADC_CS 23 // ADC chip select

// ADC constants
#define ADC_VREF 4096      // ADC reference voltage in millivolts
#define ADC_MAX_VALUE 4096 // Maximum value returned from ADC (2^bits)
#define ADC_CURRENT 0      // Curremt channel
#define ADC_VOLTAGE 1      // Voltage channel

// DAC constants
#define DAC_VOLTAGE 1 // Voltage channel
#define DAC_CURRENT 0 // Current channel

// Timer interrupt
#define ADC_AVG_INT 1000                           // ADC averaging interval (ms)
#define ADC_SAMPLE_INT 100                         // ADC sampling interval (ms)
#define MAX_SAMPLES (ADC_AVG_INT / ADC_SAMPLE_INT) // Number of samples to collect

// Conversion factors and functions (all values in millivolts and milliamps)
#define MAX_MV 30000                                                  // Maximum millivolts the supply can output
#define MAX_MA 2000                                                   // Maximum milliamps the supply can output
#define ADC_TO_RAW_VOLT(x) ((x * ADC_VREF) / ADC_MAX_VALUE)           // Convert ADC reading to actual volts seen on pin
#define ADC_TO_VOLT(x) (ADC_TO_RAW_VOLT(x) * (MAX_MV / ADC_VREF))     // Convert ADC reading to volts on supply output
#define ADC_TO_AMP(x) (MAX_MA * (ADC_TO_RAW_VOLT(x) / ADC_MAX_VALUE)) // Convert ADC reading to amps through load

// Rotary encoder constants
#define MV_PER_CLICK 10
#define MA_PER_CLICK 10

// Fan control constants
#define FAN_PWM_PIN 8          // Fan PWM control pin
#define FAN_SENSOR_PIN 0       // Fan tacho pin
#define THERM_PIN A0           // Thermistor sense pin
#define R_THERM_GROUND 10000.0 // Voltage divider resistance to ground
#define FAN_ON 30.0            // Temp where fan is turned on
#define FAN_MAX 35.0           // Temp where fan is maxed out

// Steinhart-Hart coefficients for Vishay NTCALUG03A103GC
#define THERM_R25 10000.0
#define THERM_COEFF_A 1.145241779e-3
#define THERM_COEFF_B 2.314660102e-4
#define THERM_COEFF_C 0.9841582652e-7

// Overtemp protection
#define OVERTEMP_LIMIT_ON 90  // Overtemp protection turns on
#define OVERTEMP_LIMIT_OFF 80 // Overtemp protection turns off

// Settings lock
#define LOCK_PIN 1 // Settings lock

// Display
Display display;

// Rotary encoders
RotaryEncoder currentEncoder(ROTARY_DT_2, ROTARY_CLK_2, RotaryEncoder::LatchMode::TWO03);
RotaryEncoder voltageEncoder(ROTARY_DT_1, ROTARY_CLK_1, RotaryEncoder::LatchMode::TWO03);
ControlKnob currentDial(currentEncoder, display, Display::ID::current, 0, MAX_MA, 10, 100, ROTARY_SW_2);
ControlKnob voltageDial(voltageEncoder, display, Display::ID::voltage, 0, MAX_MV, 10, 1000, ROTARY_SW_1);

// ADC
MCP3202 adc(ADC_CS);

// DAC
MCP4922 dac;

// Fan
TempControl tempControl(FAN_PWM_PIN, FAN_SENSOR_PIN, FAN_ON, FAN_MAX);

// Averaged readings
Average measVolt(MAX_SAMPLES);
Average measAmp(MAX_SAMPLES);
Average measTemp(MAX_SAMPLES);

// Voltage and current set on dials (millivolts and milliamps)
uint32_t vSet = 0.0;
uint32_t iSet = 0.0;

// Overtemp protection
bool overTemp = false;

// Settings lock
bool locked = false;

float getTemp()
{
  int v = analogRead(THERM_PIN);
  float r2 = (R_THERM_GROUND * (1023.0 / (float)v - 1.0));
  float logR2 = log(r2);
  return (1.0 / (THERM_COEFF_A + THERM_COEFF_B * logR2 + THERM_COEFF_C * logR2 * logR2 * logR2)) - 273.15;
}

void onReadADC()
{
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  measVolt.update(ADC_TO_VOLT((float)adc.readChannel(ADC_VOLTAGE)));
  measAmp.update(ADC_TO_AMP((float)adc.readChannel(ADC_CURRENT)));
  measTemp.update(getTemp());
}

void setup()
{
  pinMode(DAC_CS, OUTPUT);
  pinMode(ADC_CS, OUTPUT);
  pinMode(FAN_SENSOR_PIN, INPUT_PULLUP);
  pinMode(FAN_PWM_PIN, OUTPUT);
  pinMode(LOCK_PIN, INPUT_PULLUP);
  digitalWrite(DAC_CS, HIGH);
  digitalWrite(ADC_CS, HIGH);
  Serial.begin(115200);
  SPI.begin();

  // Connect current and voltage dial so coarse mode behaves nicely
  currentDial.setPeer(&voltageDial);
  voltageDial.setPeer(&currentDial);

  // Start timer interrupt
  ITimer3.init();
  ITimer3.attachInterruptInterval(ADC_SAMPLE_INT, onReadADC);

  // Set all DAC output voltages to zero
  dac.begin(DAC_CS);
  dac.analogWrite(0, DAC_CURRENT);
  dac.analogWrite(0, DAC_VOLTAGE);
  display.init();

  // Initialize cooling system
  tempControl.begin();
}

void loop()
{
  // Settings lock enabled?
  bool releaseLock = false;
  if (digitalRead(LOCK_PIN) == 0)
  {
    if (!locked)
    {
      display.setLockedMode(true);
      locked = true;
    }
  }
  else
  {
    if (locked)
    {
      display.setLockedMode(false);
      locked = false;
      releaseLock = true;
    }
  }

  // Overtemp? Disble all dials and keep voltage and current at 0.
  if (!overTemp)
  {
    // Read the dials
    currentDial.tick();
    voltageDial.tick();

    int32_t i = currentDial.getValue();
    int32_t v = voltageDial.getValue();

    // Dials moved?
    if (i != iSet || v != vSet || releaseLock)
    {
      vSet = v;
      iSet = i;

      // Set voltage and current
      if (!locked)
      {
        dac.analogWrite(((uint32_t)dac.maxValue() * toCalibratedVOutput(vSet)) / MAX_MV, DAC_VOLTAGE);
        dac.analogWrite(((uint32_t)dac.maxValue() * toCalibratedIOutput(iSet)) / MAX_MA, DAC_CURRENT);
      }
    }
  }

  // Handle overtemp if needed
  float temp = measTemp.getAvg();
  if (!overTemp && temp > OVERTEMP_LIMIT_ON)
  {
    overTemp = true;
    vSet = 0.0;
    iSet = 0.0;
    dac.analogWrite(0, DAC_CURRENT);
    dac.analogWrite(0, DAC_VOLTAGE);
    display.overtemp();
  }
  if (overTemp && temp < OVERTEMP_LIMIT_OFF)
  {
    overTemp = false;
    display.normal();
  }

  // Update display (only updates changed values)
  display.setVSet(vSet);
  display.setISet(iSet);
  display.setTemp(round(temp));
  if (!overTemp)
  {
    float amp = measAmp.getAvg(), volt = measVolt.getAvg();

    // We measure in relation to negative supply. Adjust for drop across sense resistor
    float calAmp = toCalibratedIReading(amp);
    float calVolt = toCalibratedVReading(volt);
    display.setVAct(calVolt);
    display.setIAct(calAmp);
    display.setPAct((calAmp * calVolt) / 1000);
  }
  display.setRpm(tempControl.getCachedSpeed());
  display.refresh();

  // Set fan speed
  tempControl.setTemp(temp);
}
