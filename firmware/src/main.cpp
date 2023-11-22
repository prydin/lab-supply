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
#include "RotaryEncoder.hpp"
#include "Display.hpp"
#include "TempControl.hpp"
#include "Average.hpp"

// Current dial pins
#define ROTARY_DT_1 12
#define ROTARY_CLK_1 11
#define ROTARY_SW_1 10

// Voltage dial pins
#define ROTARY_DT_2 7
#define ROTARY_CLK_2 5
#define ROTARY_SW_2 9

// ADC/DAC pins
#define DAC_CS 13 // DAC chip select
#define ADC_CS 23 // ADC chip select

// ADC constants
#define ADC_VREF 4.096       // ADC reference voltage
#define ADC_MAX_VALUE 4096.0 // Maximum value returned from ADC (2^bits)
#define ADC_CURRENT 0        // Curremt channel
#define ADC_VOLTAGE 1        // Voltage channel

// DAC constants
#define DAC_VOLTAGE 1 // Voltage channel
#define DAC_CURRENT 0 // Current channel

// Timer interrupt
#define ADC_AVG_INT 1000                           // ADC averaging interval (ms)
#define ADC_SAMPLE_INT 100                         // ADC sampling interval (ms)
#define MAX_SAMPLES (ADC_AVG_INT / ADC_SAMPLE_INT) // Number of samples to collect

// Conversion factors and functions
#define MAX_VOLT 30.0                                               // Maximum volts the supply can output
#define MAX_AMP 2.0                                                 // Maximum amps the supply can output
#define R_SENSE 0.5                                                 // Value of sense resistor (ohms)
#define ADC_TO_RAW_VOLT(x) ((x * ADC_VREF) / ADC_MAX_VALUE)         // Convert ADC reading to actual volts seen on pin
#define ADC_TO_VOLT(x) (ADC_TO_RAW_VOLT(x) * (MAX_VOLT / ADC_VREF)) // Convert ADC reading to volts on supply output
#define ADC_TO_AMP(x) (ADC_TO_RAW_VOLT(x) * (MAX_AMP / ADC_VREF))   // Convert ADC reading to amps through load

// Fan control constants
#define FAN_PWM_PIN 6          // Fan PWM control pin
#define FAN_SENSOR_PIN 0       // Fan tacho pin
#define THERM_PIN A2           // Thermistor sense pin
#define R_THERM_GROUND 10000.0 // Voltage divider resistance to ground
#define FAN_ON 30.0            // Temp where fan is turned on
#define FAN_MAX 85.0           // Temp where fan is maxed out

// Steinhart-Hart coefficients for Vishay NTCALUG03A103GC
#define THERM_R25 10000.0
#define THERM_COEFF_A 1.145241779e-3
#define THERM_COEFF_B 2.314660102e-4
#define THERM_COEFF_C 0.9841582652e-7

// Overtemp protection
#define OVERTEMP_LIMIT_ON 90  // Overtemp protection turns on
#define OVERTEMP_LIMIT_OFF 80 // Overtemp protection turns off

// Rotary encoders
RotaryEncoder currentDial(ROTARY_DT_1, ROTARY_CLK_1, ROTARY_SW_1, 0, 200, 1, 10);
RotaryEncoder voltageDial(ROTARY_DT_2, ROTARY_CLK_2, ROTARY_SW_2, 0, 3000, 1, 50);

// ADC
MCP3202 adc(ADC_CS);

// DAC
MCP4922 dac;

// Display
Display display;

// Fan
TempControl tempControl(FAN_PWM_PIN, FAN_SENSOR_PIN, FAN_ON, FAN_MAX);

// Averaged readings
Average measVolt(MAX_SAMPLES);
Average measAmp(MAX_SAMPLES);
Average measTemp(MAX_SAMPLES);

// Voltage and current set on dials
float vSet = 0.0;
float iSet = 0.0;

// Overtemp protection
bool overTemp = false;

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
  measVolt.update(ADC_TO_VOLT(adc.readChannel(ADC_VOLTAGE)));
  measAmp.update(ADC_TO_AMP(adc.readChannel(ADC_CURRENT)));
  measTemp.update(getTemp());
}

void onPollRotary()
{
  currentDial.service();
  voltageDial.service();
}

void setup()
{
  pinMode(DAC_CS, OUTPUT);
  pinMode(ADC_CS, OUTPUT);
  pinMode(FAN_SENSOR_PIN, INPUT_PULLUP);
  pinMode(FAN_PWM_PIN, OUTPUT);
  digitalWrite(DAC_CS, HIGH);
  digitalWrite(ADC_CS, HIGH);
  Serial.begin(9600);
  SPI.begin();

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
  // Overtemp? Disble all dials and keep voltage and current at 0.
  if (!overTemp)
  {
    // Read the dials
    currentDial.service();
    voltageDial.service();

    // Dials moved?
    if (currentDial.getChange() || voltageDial.getChange())
    {
      vSet = (float)voltageDial.getCount() / 100.0;
      iSet = (float)currentDial.getCount() / 100.0;

      // Set voltage
      dac.analogWrite(round((vSet / MAX_VOLT) * (float)dac.maxValue()), DAC_VOLTAGE);

      // Set current
      dac.analogWrite(round((iSet / MAX_AMP) * (float)dac.maxValue()), DAC_CURRENT);
    }
  }

  // Handle overtemp if needed
  float temp = measTemp.getAvg();
  if (!overTemp && temp > OVERTEMP_LIMIT_ON)
  {
    overTemp = true;
    vSet = 0.0;
    iSet = 0.0;
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
    display.setIAct(amp);

    // We measure in relation to negative supply. Adjust for drop across sense resistor
    float vAdj = volt - amp * R_SENSE;
    display.setVAct(vAdj);
    display.setPAct(amp * vAdj);
  }
  display.setRpm(tempControl.getCachedSpeed());
  display.refresh();

  // Set fan speed
  tempControl.setTemp(temp);
}
