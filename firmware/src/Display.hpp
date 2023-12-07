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
#include <LiquidCrystal_I2C.h>

// Bits in the change bitmap
#define ISET_CHANGED 1
#define IACT_CHANGED 2
#define VSET_CHANGED 4
#define VACT_CHANGED 8
#define TEMP_CHANGED 16
#define PACT_CHANGED 32
#define RPM_CHANGED 64

class Display
{
    LiquidCrystal_I2C lcd;
    uint16_t changed = 0xffff; // Update everything on init
    float vSet = 0.0, vAct = 0.0, iSet = 0.0, iAct = 0.0, temp = 0.0, pAct = 0.0;
    uint16_t rpm = 0;
    char convBuf[100]; // Buffer used during number to string conversions

public:
    Display() : lcd(0x27, 20, 4)
    {
    }

    void init()
    {
        lcd.init();
        lcd.backlight();
        normal();
    }

    void normal()
    {
        lcd.clear();
        /// Draw an initial display like this:
        // V= 0.01V ->  0.00V
        // I= 1.23A ->  1.11A
        // T= 23.0°C    P=55.5W
        // FAN=1234RPM
        lcd.setCursor(0, 0);
        lcd.print("V=  0.00V ->   0.00V");
        lcd.setCursor(0, 1);
        lcd.print("I=  0.00A ->   0.00A");
        lcd.setCursor(0, 2);
        lcd.print("T= ----\xdf\x43  P= --.--W");
        lcd.setCursor(0, 3);
        lcd.print("FAN  ---RPM");
    }

    void overtemp()
    {
        lcd.clear();
        /// Draw an initial display like this:
        // *** OVERTEMP! ***
        // Allow to cool off!
        // T= 23.0°C    P=55.5W
        // FAN=1234RPM
        lcd.setCursor(0, 0);
        lcd.print("*** OVERTEMP! ***");
        lcd.setCursor(0, 1);
        lcd.print("Allow to cool off!");
        lcd.setCursor(0, 2);
        lcd.print("T= ----\xdf\x43");
        lcd.setCursor(0, 3);
        lcd.print("FAN  ---RPM");
    }

    void setISet(float v)
    {
        if (v == iSet)
        {
            return;
        }
        iSet = v;
        changed |= ISET_CHANGED;
    }

    void setIAct(float v)
    {
        if (v == iAct)
        {
            return;
        }
        iAct = v;
        changed |= IACT_CHANGED;
    }

    void setVSet(float v)
    {
        if (v == vSet)
        {
            return;
        }
        vSet = v;
        changed |= VSET_CHANGED;
    }

    void setVAct(float v)
    {
        if (v == vAct)
        {
            return;
        }
        vAct = v;
        changed |= VACT_CHANGED;
    }

    void setTemp(float v)
    {
        if (v == temp)
        {
            return;
        }

        temp = v;
        changed |= TEMP_CHANGED;
    }

    void setPAct(float v)
    {
        if (v == pAct)
        {
            return;
        }
        pAct = v;
        changed |= PACT_CHANGED;
    }

    void setRpm(int v)
    {
        if (v == rpm)
        {
            return;
        }
        rpm = v;
        changed |= RPM_CHANGED;
    }

    void refresh()
    {
        if (changed & VSET_CHANGED)
        {
            printReading(3, 0, vSet);
        }
        if (changed & VACT_CHANGED)
        {
            printReading(14, 0, vAct);
        }

        if (changed & ISET_CHANGED)
        {
            printReading(3, 1, iSet);
        }
        if (changed & IACT_CHANGED)
        {
            printReading(14, 1, iAct);
        }
        if (changed & TEMP_CHANGED)
        {
            printInt(3, 2, round(temp), 5);
        }
        if (changed & PACT_CHANGED)
        {
            printReading(14, 2, pAct);
        }
        if (changed & RPM_CHANGED)
        {
            printInt(4, 3, rpm, 4);
        }
        changed = 0;
    }

private:
    void printReading(int x, int y, float r)
    {
        lcd.setCursor(x, y);
        if (r > 99.0 || r < 0)
        {
            lcd.print("--.--");
        }
        else
        {
            dtostrf(r, 5, 2, convBuf);
            lcd.print(convBuf);
        }
    }

    void printInt(int x, int y, int r, int size)
    {
        lcd.setCursor(x, y);
        dtostrf(r, size, 0, convBuf);
        lcd.print(convBuf);
    }
};