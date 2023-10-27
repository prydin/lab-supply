#import <LiquidCrystal.h>
#import <SPI.h>
#include <stdio.h>

// Current dial pins
#define ROTARY_DT_1   11
#define ROTARY_CLK_1  12
#define ROTARY_SW_1   10

// Voltage dial pins
#define ROTARY_DT_2   7
#define ROTARY_CLK_2  5
#define ROTARY_SW_2   9

// ADC/DAC pins
#define DAC_CS        13 // High -> ACC, Low -> DAC
#define LDAC          1

#define LCD_RS        12
#define LCD_RW        11
#define LCD_E         10
#define LCD_DB0       9
#define LCD_DB1       7
#define LCD_DB2       5
#define LCD_DB3       23

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
//LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_E, LCD_DB0, LCD_DB1, LCD_DB2, LCD_DB3);


void setup() {
  pinMode(DAC_CS, OUTPUT);  
  pinMode(LDAC, OUTPUT);
  digitalWrite(DAC_CS, HIGH);
  digitalWrite(LDAC, HIGH);
  Serial.begin(9600);
  SPI.begin();

  // Set all DAC output voltages to zero
  setDACVoltage(0, 0);
  setDACVoltage(1, 0);

  /*lcd.begin(16, 2);
  lcd.clear();
  lcd.cursor();
  lcd.print("Hellow world!");  */
}

void setDACVoltage(int channel, int v) {
  uint16_t command = (channel & 1) << 15 | 0x7000 | v;
  Serial.print("Command: " );
  Serial.print(command);
  Serial.print(" channel: ");
  Serial.println((channel & 1) << 15);
  digitalWrite(DAC_CS, LOW); // Select the DAC
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
 
  digitalWrite(15, LOW);

  SPI.transfer(command >> 8);
  SPI.transfer(command & 0xff);
  SPI.endTransaction();
  delayMicroseconds(1); // Give things some time to settle
  digitalWrite(DAC_CS, HIGH); // Make sure no one accidentally sends commands to the DAC
  digitalWrite(LDAC, LOW); // Send LDAC pulse to activate output
  delayMicroseconds(2);
  digitalWrite(LDAC, HIGH); // Load the DAC latch to output the selected voltage
}

void loop() {
  currentDial.service();
  voltageDial.service();
  // Serial.println(voltageDial.get_count());
  if (currentDial.get_change() || voltageDial.get_change()) {
    float v_set = (float) voltageDial.get_count() / 100.0;
    float i_set = (float) currentDial.get_count() / 100.0;

    // Set voltage
    Serial.println((v_set / 30.0) * 4096.0);
    setDACVoltage(0, round((v_set / 30.0) * 4096.0));

    // Set current
    setDACVoltage(1, round((i_set / 2.0) * 4096.0));

    char buffer[50];
   
    char vbuf[6];
    char ibuf[6];
    dtostrf(v_set, 2, 2, vbuf);
    dtostrf(i_set, 2, 2, ibuf);
    sprintf(buffer, "V=%s I=%s", vbuf, ibuf);
    Serial.println(buffer);
  }
}
