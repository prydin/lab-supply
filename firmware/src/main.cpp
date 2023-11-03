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

// Current dial pins
#define ROTARY_DT_1 11
#define ROTARY_CLK_1 12
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

// Rotary encoders
RotaryEncoder currentDial(ROTARY_DT_1, ROTARY_CLK_1, ROTARY_SW_1, 0, 200, 1, 10);
RotaryEncoder voltageDial(ROTARY_DT_2, ROTARY_CLK_2, ROTARY_SW_2, 0, 3000, 1, 50);

// ADC
MCP3202 adc(ADC_CS);

// DAC
MCP4922 dac;

// Display
Display display;

volatile float currSum = 0.0;
volatile float voltSum = 0.0;
volatile float currAvg = 0.0;
volatile float voltAvg = 0.0;
int nSamples = 0;
float vSet = 0.0;
float iSet = 0.0;

void onReadADC()
{
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  voltSum += ADC_TO_VOLT(adc.readChannel(ADC_VOLTAGE));
  currSum += ADC_TO_AMP(adc.readChannel(ADC_CURRENT));
  SPI.endTransaction();
  nSamples++;
  if (nSamples > MAX_SAMPLES)
  {
    voltAvg = voltSum / (float)nSamples;
    currAvg = currSum / (float)nSamples;
    voltSum = 0;
    currSum = 0;
    nSamples = 0;
  }
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
}

void loop()
{
  currentDial.service();
  voltageDial.service();

  if (currentDial.getChange() || voltageDial.getChange())
  {
    vSet = (float)voltageDial.getCount() / 100.0;
    iSet = (float)currentDial.getCount() / 100.0;

    // Set voltage
    dac.analogWrite(round((vSet / MAX_VOLT) * (float)dac.maxValue()), DAC_VOLTAGE);

    // Set current
    dac.analogWrite(round((iSet / MAX_AMP * R_SENSE) * (float)dac.maxValue()), DAC_CURRENT);
  }

  // Update display (onlh updates changed values)
  display.setVSet(vSet);
  display.setISet(iSet);
  display.setPSet(iSet * vSet);
  display.setIAct(currAvg);
  display.setVAct(voltAvg);
  display.setPAct(currAvg * voltAvg);
  display.refresh();
}