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
#define PSET_CHANGED 16
#define PACT_CHANGED 32

class Display
{
    LiquidCrystal_I2C lcd;
    uint16_t changed = 0xffff; // Update everything on init
    float vSet = 0.0, vAct = 0.0, iSet = 0.0, iAct = 0.0, pSet = 0.0, pAct = 0.0;

public:
    Display() : lcd(0x27, 20, 4)
    {
    }

    void init()
    {
        lcd.init();
        lcd.backlight();
        lcd.clear();

        /// Draw an initial display like this:
        //
        //    Set     Actual
        //    --------------
        // V   0.01V   0.00V
        // I   1.23A   1.11A
        // P   0.01W   0.00W
        lcd.setCursor(0, 0);
        lcd.print("      Set   Actual");
        lcd.setCursor(0, 1);
        lcd.print("V  --.--V   --.--V");
        lcd.setCursor(0, 2);
        lcd.print("I  --.--A   --.--A");
        lcd.setCursor(0, 3);
        lcd.print("P  --.--W   --.--W");
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

    void setPSet(float v)
    {
        if (v == pSet)
        {
            return;
        }

        pSet = v;
        changed |= PSET_CHANGED;
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

    void refresh()
    {
        if (changed & VSET_CHANGED)
        {
            printReading(3, 1, vSet);
        }
        if (changed & VACT_CHANGED)
        {
            printReading(12, 1, vAct);
        }

        if (changed & ISET_CHANGED)
        {
            printReading(3, 2, iSet);
        }
        if (changed & IACT_CHANGED)
        {
            printReading(12, 2, iAct);
        }
        if (changed & PSET_CHANGED)
        {
            printReading(3, 3, pSet);
        }
        if (changed & PACT_CHANGED)
        {
            printReading(12, 3, pAct);
        }
        changed = 0;
    }

private:
    void printReading(int x, int y, float r)
    {
        lcd.setCursor(x, y);
        char buffer[10];
        if (r > 99.0 || r < 0)
        {
            lcd.print("--.--");
        }
        else
        {
            if (r < 10.0)
            {
                lcd.print(" ");
            }
            dtostrf(r, 2, 2, buffer);
            lcd.print(buffer);
        }
    }
};