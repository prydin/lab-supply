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
#include <Arduino.h>

class RotaryEncoder
{
public:
    RotaryEncoder(int8_t phase1, int8_t phase2, int8_t sw, int16_t min, int16_t max, int16_t fine, int16_t coarse, bool reversed = false)
        : m_phase1(phase1),
          m_phase2(phase2),
          m_sw(sw),
          m_max(max * 2),
          m_min(min * 2),
          m_fine(fine),
          m_coarse(coarse),
          m_reversed(reversed),
          m_change(0),
          m_previous_state(0),
          m_steps(0)
    {
        pinMode(phase1, INPUT_PULLUP);
        pinMode(phase2, INPUT_PULLUP);
        pinMode(sw, INPUT_PULLUP);
        m_previous_state = pin_state();
        m_change = 0;
    }
    ~RotaryEncoder() {}

    int8_t pin_state()
    {
        int8_t state_now = 0;
        if (digitalRead(m_phase1) == LOW)
        {
            state_now |= 2;
        }
        if (digitalRead(m_phase2) == LOW)
        {
            state_now |= 1;
        }
        return state_now;
    }

    void service()
    {
        int8_t state_now = pin_state();
        state_now ^= state_now >> 1; // two bit gray-to-binary
        int8_t difference = m_previous_state - state_now;
        // bit 1 has the direction, bit 0 is set if changeed
        if (difference & 1)
        {
            m_previous_state = state_now;
            int delta = (difference & 2) - 1;
            if (m_reversed)
            {
                delta = -delta;
            }
            m_change += delta;
            m_steps += delta * (isPressed() ? m_coarse : m_fine);
            if (m_steps > m_max)
            {
                m_steps = m_max;
            }
            if (m_steps < m_min)
            {
                m_steps = m_min;
            }
        }
    }

    bool isPressed()
    {
        return digitalRead(m_sw) == LOW;
    }

    int8_t getChange()
    {
        uint8_t sreg = SREG; // save the current interrupt enable flag
        noInterrupts();
        int8_t change = m_change;
        m_change &= 1;
        change >>= 1;
        SREG = sreg; // restore the previous interrupt enable flag state
        return change;
    }

    int getCount()
    {
        return m_steps >> 1;
    }

private:
    const int8_t m_phase1;
    const int8_t m_phase2;
    const int8_t m_sw;
    const int16_t m_max;
    const int16_t m_min;
    const float m_fine;
    const float m_coarse;
    const bool m_reversed;
    volatile int16_t m_change;
    volatile int16_t m_previous_state;
    volatile int16_t m_steps;
};