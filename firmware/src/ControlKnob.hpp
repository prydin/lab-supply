#include <RotaryEncoder.h>
#include "Display.hpp"

class ControlKnob
{
public:
    ControlKnob(RotaryEncoder &knob, Display &display, Display::ID id, int32_t minValue, int32_t maxValue, int16_t slowIncrement, int16_t fastIncrement, int16_t switchPin)
        : knob(knob), display(display), id(id), minValue(minValue), maxValue(maxValue), slowIncrement(slowIncrement), fastIncrement(fastIncrement), switchPin(switchPin)
    {
        pinMode(switchPin, INPUT_PULLUP);
    }

    void tick();

    int32_t getValue()
    {
        return currentValue;
    }

    bool isFast()
    {
        return fast;
    }

    void inline setPeer(ControlKnob *p)
    {
        peer = p;
    }

private:
    ControlKnob *peer;
    RotaryEncoder &knob;
    Display &display;
    int32_t minValue;
    int32_t maxValue;
    int16_t slowIncrement;
    int16_t fastIncrement;
    int16_t switchPin;

    int16_t currentPos = 0;
    bool fast = false;
    bool pressed = false;
    int32_t currentValue = 0;
    Display::ID id;
};