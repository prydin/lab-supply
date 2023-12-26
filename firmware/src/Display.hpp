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

#ifndef __DISPLAY_HPP
#define __DISPLAY_HPP
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
public:
    enum ID
    {
        voltage,
        current
    };

    const char lockChar[8] = {
        0b00100,
        0b01010,
        0b10001,
        0b11111,
        0b10001,
        0b10101,
        0b11111,
        0b00000};

    Display();

    void init();

    void normal();

    void overtemp();

    void setISet(int32_t v);

    void setIAct(int32_t v);

    void setVSet(int32_t v);

    void setVAct(int32_t v);

    void setTemp(int32_t v);

    void setPAct(int32_t v);

    void setRpm(int32_t v);

    void setCoarseMode(ID id, bool b);

    void refresh();

private:
    LiquidCrystal_I2C lcd;
    uint16_t changed = 0xffff; // Update everything on init
    int32_t vSet = 0.0, vAct = 0.0, iSet = 0.0, iAct = 0.0, temp = 0.0, pAct = 0.0, rpm = 0;
    char convBuf[100]; // Buffer used during number to string conversions
    uint8_t cursorX;
    uint8_t cursorY;
    bool cursorActive;

    void printReading(int x, int y, uint32_t r);

    void printInt(int x, int y, int r, int size);
};
#endif