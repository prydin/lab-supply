#include <math.h>
#include <Arduino.h>

// Voltage output calibration
float vOutCal[] = {0.014,
                   0.998,
                   1.987,
                   2.971,
                   3.93,
                   4.951,
                   5.929,
                   6.907,
                   7.899,
                   8.873,
                   9.852,
                   10.836,
                   11.810,
                   12.785,
                   13.768,
                   14.747,
                   15.732,
                   16.704,
                   17.694,
                   18.683,
                   19.667,
                   20.638,
                   21.621,
                   22.604,
                   23.584,
                   24.551,
                   25.529,
                   26.511,
                   27.500,
                   28.471,
                   29.447};

inline float lerp(float y1, float y2, float dx, float y)
{
    return y * ((y2 - y1) / dx);
}

float toCalibrated(float raw, float table[], int n, float maxValue)
{
    float scale = maxValue / ((float)n - 1);
    if (raw > maxValue || raw < 0)
    {
        return raw;
    }
    int idx = (int)floor(raw / scale);
    if (idx >= n)
    {
        return raw;
    }
    float nearest = idx * scale;
    float error = table[idx] - nearest;

    // Increas precision using linear interpolation (unless we're at the end of the table)
    if (idx < n - 1)
    {
        float next = (idx + 1) * scale;
        error += lerp(table[idx] - nearest, table[idx + 1] - next, scale, raw - nearest);
        Serial.println(table[idx] - nearest);
        Serial.println(idx);
        Serial.println(next);
    }
    return fmax(0, raw - error);
}

uint32_t toCalibrateVOutput(uint32_t v)
{
    return (uint32_t)(toCalibrated(((float)v) / 1000, vOutCal, sizeof(vOutCal) / sizeof(float), 30.0) * 1000);
}
