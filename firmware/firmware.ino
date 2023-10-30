#import <LiquidCrystal_I2C.h>
#import <SPI.h>
#import <MCP3202.h>
#include <Print.h>
#include <stdio.h>
#define USE_TIMER_3 true
#include <TimerInterrupt.h>
#include <ISR_Timer.h>

// Current dial pins
#define ROTARY_DT_1   11
#define ROTARY_CLK_1  12
#define ROTARY_SW_1   10

// Voltage dial pins
#define ROTARY_DT_2   7
#define ROTARY_CLK_2  5
#define ROTARY_SW_2   9

// Conversion factors
#define ADC_TO_VOLT(x)  ((x / 4096) * 30)
#define ADC_TO_AMP(x)   ((x / 4096) * 2)

// ADC/DAC pins
#define DAC_CS        13    // DAC chip select
#define ADC_CS        23    // ADC chip select

// Timer interrupt
#define ADC_AVG_INT     1000   // ADC averaging interval (ms)
#define ADC_SAMPLE_INT  10    // ADC sampling interval (ms)
#define MAX_SAMPLES     (ADC_AVG_INT / ADC_SAMPLE_INT)
class RotaryEncoder {
  public:
    RotaryEncoder(int8_t phase1, int8_t phase2, int8_t sw, int16_t min, int16_t max, int16_t fine, int16_t coarse, bool reversed = false) : 
        m_phase1(phase1), 
        m_phase2(phase2),
        m_fine(fine), 
        m_coarse(coarse),
        m_max(max * 2),
        m_min(min * 2),
        m_sw(sw),
        m_reversed(reversed) {
      pinMode(phase1, INPUT_PULLUP);
      pinMode(phase2, INPUT_PULLUP);
      pinMode(sw, INPUT_PULLUP);
      m_previous_state = pin_state();
      m_change = 0;
    }
    ~RotaryEncoder() {}

  int8_t pin_state() {
    int8_t state_now = 0;
    if (digitalRead(m_phase1) == LOW) {
      state_now |= 2;
    }
    if (digitalRead(m_phase2) == LOW) {
      state_now |= 1;
    }
    return state_now;
  }

  void service() {
    int8_t state_now = pin_state();
    state_now ^= state_now >> 1;  // two bit gray-to-binary
    int8_t difference = m_previous_state - state_now;
    // bit 1 has the direction, bit 0 is set if changeed
    if (difference & 1) {
      m_previous_state = state_now;
      int delta = (difference & 2) - 1;
      if (m_reversed) {
        delta = -delta;
      }
      m_change += delta;
      m_steps += delta * (is_pressed() ? m_coarse : m_fine);
      if(m_steps > m_max) {
        m_steps = m_max;
      }
      if(m_steps < m_min) {
        m_steps = m_min;
      }
    }
  }

  bool is_pressed() {
    return digitalRead(m_sw) == LOW;
  }

   int8_t get_change() {
    uint8_t sreg = SREG;  // save the current interrupt enable flag
    noInterrupts();
    int8_t change = m_change;
    m_change &= 1;
    change >>= 1;
    SREG = sreg;  // restore the previous interrupt enable flag state
    return change;
  }

  int get_count() {
    return m_steps >> 1;
  }

  private:
    int8_t m_phase1 = 0;
    int8_t m_phase2 = 0;
    int8_t m_sw = 0;
    float m_coarse = 1.0;
    float m_fine = 1.0;
    bool m_reversed = false;
    volatile int m_change = 0;
    int8_t m_previous_state = 0;
    int m_steps = 0;
    int16_t m_max = 0;
    int16_t m_min = 0;
};

RotaryEncoder currentDial(ROTARY_DT_1, ROTARY_CLK_1, ROTARY_SW_1, 0, 200, 1, 10);
RotaryEncoder voltageDial(ROTARY_DT_2, ROTARY_CLK_2, ROTARY_SW_2, 0, 3000, 1, 50);
MCP3202 adc(ADC_CS);
LiquidCrystal_I2C lcd(0x27, 20, 4);
volatile float currSum = 0.0;
volatile float voltSum = 0.0;
volatile float currAvg = 0.0;
volatile float voltAvg = 0.0;
volatile bool refreshReadings = false;
int nSamples = 0;
float v_set = 0.0;
float i_set = 0.0;


void setup() {
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
  setDACVoltage(0, 0);
  setDACVoltage(1, 0);

lcd.init();
lcd.backlight();
lcd.clear();
displayStatic();
}

void setDACVoltage(int channel, int v) {
  uint16_t command = (channel & 1) << 15 | 0x7000 | v;
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
 
  digitalWrite(15, LOW);

  digitalWrite(DAC_CS, LOW); 
  SPI.transfer(command >> 8);
  SPI.transfer(command & 0xff);
  SPI.endTransaction();
  delayMicroseconds(1); // Give things some time to settle
  digitalWrite(DAC_CS, HIGH); 
}

void printReading(int x, int y, float r, char unit) {
  lcd.setCursor(x, y);
  char buffer[10];
  if(r > 99.0 | r < 0) {
    lcd.print("--.--");
  } else {
    if(r < 10.0) {
      lcd.print(" ");
    }
    dtostrf(r, 2, 2, buffer);
    lcd.print(buffer);
  }
  lcd.print(unit);
}


void displayStatic() {
  //    Set     Actual
    //    --------------
    // V   0.01V   0.00V
    // I   1.23A.  1.11A
    lcd.setCursor(0, 0);
    lcd.print("    Set     Actual");
    lcd.setCursor(0, 1);
    lcd.print("    --------------");
    lcd.setCursor(0, 2);
    lcd.print("V  --.--V   --.--V");
    lcd.setCursor(0, 3);
    lcd.print("I  --.--A   --.--A");
}

void loop() {
  // Read rotary encoders
  currentDial.service();
  voltageDial.service();

  if (currentDial.get_change() || voltageDial.get_change()) {
    v_set = (float) voltageDial.get_count() / 100.0;
    i_set = (float) currentDial.get_count() / 100.0;

    // Set voltage
    setDACVoltage(0, round((v_set / 30.0) * 4096.0));

    // Set current
    setDACVoltage(1, round((i_set / 2.0) * 4096.0));

    // Update display
    printReading(3, 2, v_set, 'V');
    printReading(3, 3, i_set, 'A');
    Serial.println(voltSum);
  } 
  if(refreshReadings) {
     printReading(12, 2, ADC_TO_VOLT(voltAvg), 'V');
     printReading(12, 3, ADC_TO_AMP(currAvg), 'A');
     refreshReadings = false;
  }
}

void onReadADC() {
  voltSum += adc.readChannel(0);
  currSum += adc.readChannel(1);
  nSamples++;
  if(nSamples > MAX_SAMPLES) {
    voltAvg = voltSum / (float) nSamples;
    currSum  = voltSum / (float) nSamples;
    voltSum = 0;
    currSum = 0;
    nSamples = 0;
    refreshReadings = true;
  }
}

