#include "Display.hpp"

Display::Display() : lcd(0x27, 20, 4)
{
}

void Display::init()
{
    lcd.init();
    lcd.createChar(0, lockChar);
    lcd.backlight();
    normal();
}

void Display::normal()
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

void Display::overtemp()
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

void Display::setISet(int32_t v)
{
    if (v == iSet)
    {
        return;
    }
    iSet = v;
    changed |= ISET_CHANGED;
}

void Display::setIAct(int32_t v)
{
    if (v == iAct)
    {
        return;
    }
    iAct = v;
    changed |= IACT_CHANGED;
}

void Display::setVSet(int32_t v)
{
    if (v == vSet)
    {
        return;
    }
    vSet = v;
    changed |= VSET_CHANGED;
}

void Display::setVAct(int32_t v)
{
    if (v == vAct)
    {
        return;
    }
    vAct = v;
    changed |= VACT_CHANGED;
}

void Display::setTemp(int32_t v)
{
    if (v == temp)
    {
        return;
    }

    temp = v;
    changed |= TEMP_CHANGED;
}

void Display::setPAct(int32_t v)
{
    if (v == pAct)
    {
        return;
    }
    pAct = v;
    changed |= PACT_CHANGED;
}

void Display::setRpm(int32_t v)
{
    if (v == rpm)
    {
        return;
    }
    rpm = v;
    changed |= RPM_CHANGED;
}

void Display::refresh()
{
    if (changed && cursorActive)
    {
        lcd.noCursor();
    }
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
        printInt(3, 2, temp, 5);
    }
    if (changed & PACT_CHANGED)
    {
        printReading(14, 2, pAct);
    }
    if (changed & RPM_CHANGED)
    {
        printInt(4, 3, rpm, 4);
    }
    if (changed && cursorActive)
    {
        lcd.setCursor(cursorX, cursorY);
        lcd.cursor();
    }
    changed = 0;
}

void Display::setCoarseMode(ID id, bool b)
{
    if (b)
    {
        if (id == ID::voltage)
        {
            cursorX = 4;
            cursorY = 0;
        }
        else
        {
            cursorX = 6;
            cursorY = 1;
        }
        cursorActive = true;
        lcd.setCursor(cursorX, cursorY);
        lcd.cursor();
        lcd.blink();
    }
    else
    {
        lcd.noCursor();
        lcd.noBlink();
        cursorActive = false;
    }
}

void Display::printReading(int x, int y, uint32_t r)
{
    // r is in milli-units
    lcd.setCursor(x, y);
    if (r > 99000.0 || r < 0)
    {
        lcd.print("--.--");
    }
    else
    {
        dtostrf((float)r / 1000.0, 5, 2, convBuf);
        lcd.print(convBuf);
    }
}

void Display::printInt(int x, int y, int r, int size)
{
    lcd.setCursor(x, y);
    dtostrf(r, size, 0, convBuf);
    lcd.print(convBuf);
}
