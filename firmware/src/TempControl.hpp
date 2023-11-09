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
#include <FanController.h>
#include <EEPROM.h>

#define ABORT_THRESHOLD 100000    // Microseconds before we give up looking for a flank
#define SPEED_CHECK_PERIOD 500000 // Microseconds between RPM checks

class TempControl
{
private:
    uint8_t pwmPin;
    uint8_t sensePin;
    float onTemp;
    float maxTemp;
    float slope;
    bool failsafe = false;
    uint16_t cachedSpeed = 0;
    uint32_t lastSpeedReading = 0;

    void setSpeed(uint8_t speed)
    {
        analogWrite(pwmPin, speed);
    }

public:
    TempControl(uint8_t pwmPin, uint8_t sensePin, float onTemp, float maxTemp) : pwmPin(pwmPin), sensePin(sensePin), onTemp(onTemp), maxTemp(maxTemp), slope(255.0 / (maxTemp - onTemp))
    {
    }

    void begin()
    {
    }

    void setTemp(float t)
    {
        // Check for unreiable temp readings
        // TODO: Also check this against current readings. E.g. running at 20C @ 2A wouldn't be realistic.
        if (t < 5)
        {
            failsafe = true;
        }
        if (failsafe || t > maxTemp)
        {
            setSpeed(255);
            return;
        }
        float speed = t > onTemp ? min(t * slope, 255.0) : 0;
        Serial.println(speed);
        setSpeed(speed);
    }

    uint16_t getCachedSpeed()
    {
        uint32_t now = micros();
        if (now - lastSpeedReading > SPEED_CHECK_PERIOD)
        {
            lastSpeedReading = now;
            cachedSpeed = getSpeed();
        }
        return cachedSpeed;
    }

    uint16_t getSpeed()
    {
        // Simplistic algorithm not using interrupts. Good enough for 5% precision
        // and no risk of disturbing other things requiring interrupts.
        //
        // Wait for a transition. It doesn't matter if it's positive or negative
        uint8_t state = digitalRead(sensePin);
        uint32_t start = micros();
        while (digitalRead(sensePin) == state)
        {
            if (micros() - start > ABORT_THRESHOLD)
            {
                return 0;
            }
        }

        // Find the opposite flank.
        start = micros();
        while (digitalRead(sensePin) != state)
        {
            if (micros() - start > ABORT_THRESHOLD)
            {
                return 0;
            }
        }

        // Finally look for a flank that takes us back to the initial state.
        // We've now completed a perios.
        while (digitalRead(sensePin) == state)
        {
            if (micros() - start > ABORT_THRESHOLD)
            {
                return 0;
            }
        }
        uint32_t now = micros();
        uint32_t period = now - start;
        uint16_t speed = 60.0e6 / (float)period;
        return speed;
    }
};
