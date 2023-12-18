#include <Arduino.h>
#include "ControlKnob.hpp"

void ControlKnob::tick()
{
    knob.tick();
    // Serial.println(digitalRead(switchPin));
    if (digitalRead(switchPin) == LOW)
    {
        if (!pressed)
        {
            fast = !fast;
            if (fast)
            {
                if (peer)
                {
                    // Only one knob can be in fast mode at a time
                    peer->fast = false;
                }
                display.setCoarseMode(id, true);
            }
            else
            {
                display.setCoarseMode(id, false);
            }
            pressed = true;
        }
    }
    else
    {
        pressed = false;
    }
    int16_t pos = knob.getPosition();
    if (pos == currentPos)
    {
        return;
    }

    // The knob was moved
    int16_t delta = pos - currentPos;
    currentPos = pos;
    int32_t increment = fast ? fastIncrement : slowIncrement;
    int32_t newValue = currentValue + delta * increment;
    if (newValue >= minValue && newValue <= maxValue)
    {
        currentValue = newValue;
    }

    Serial.print(pos);
    Serial.print(" ");
    Serial.print(increment);
    Serial.print(" ");
    Serial.println(currentValue);
}