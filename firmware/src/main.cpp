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
#define ADC_VREF 4.096
#define ADC_MAX_VALUE 4096.0
#define ADC_CURRENT 0
#define ADC_VOLTAGE 1

// DAC constants
#define DAC_VOLTAGE 1
#define DAC_CURRENT 0

// Timer interrupt
#define ADC_AVG_INT 1000   // ADC averaging interval (ms)
#define ADC_SAMPLE_INT 100 // ADC sampling interval (ms)
#define MAX_SAMPLES (ADC_AVG_INT / ADC_SAMPLE_INT)
#define POLL_ROTARY_INT 10 // Rotary switch polling interval (ms)

// Conversion factors
#define MAX_VOLT 30.0
#define MAX_AMP 2.0
#define R_SENSE 0.5
#define ADC_TO_RAW_VOLT(x) ((x * ADC_VREF) / ADC_MAX_VALUE)
#define ADC_TO_VOLT(x) (ADC_TO_RAW_VOLT(x) * (MAX_VOLT / ADC_VREF))
#define ADC_TO_AMP(x) (ADC_TO_RAW_VOLT(x) * (MAX_AMP / ADC_VREF))

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
float v_set = 0.0;
float i_set = 0.0;

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
  // ITimer3.attachInterruptInterval(POLL_ROTARY_INT, onPollRotary);

  // Set all DAC output voltages to zero
  dac.begin(DAC_CS);
  dac.analogWrite(0, DAC_CURRENT);
  dac.analogWrite(0, DAC_VOLTAGE);

  display.init();
}

void loop()
{
  // Read rotary encoders
  // onReadADC();
  // Serial.println(adc.readChannel(0));
  // Serial.println(adc.readChannel(1));
  // Serial.print(ADC_TO_RAW_VOLT(adc.readChannel(1)));
  // Serial.print(" -> ");
  // Serial.println(voltAvg);
  currentDial.service();
  voltageDial.service();

  if (currentDial.getChange() || voltageDial.getChange())
  {
    v_set = (float)voltageDial.getCount() / 100.0;
    i_set = (float)currentDial.getCount() / 100.0;

    // Set voltage
    dac.analogWrite(round((v_set / MAX_VOLT) * (float)dac.maxValue()), DAC_VOLTAGE);

    // Set current
    dac.analogWrite(round((i_set / MAX_AMP * R_SENSE) * (float)dac.maxValue()), DAC_CURRENT);
  }

  // Update display (onlh updates changed values)
  display.setVSet(v_set);
  display.setISet(i_set);
  display.setPSet(i_set * v_set);
  display.setIAct(currAvg);
  display.setVAct(voltAvg);
  display.setPAct(currAvg * voltAvg);
  display.refresh();
}