#include <math.h>
#include <Arduino.h>

// Voltage output calibration
float vOutCal[] = {
    0.014,
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

// Current output calibration
float iOutCal[] = {
    0.001,
    0.108,
    0.235,
    0.361,
    0.454,
    0.529,
    0.613,
    0.712,
    0.812,
    0.908,
    1.04,
    1.130,
    1.250,
    1.3,
    1.4,
    1.5,
    1.6,
    1.7,
    1.8,
    1.9,
    2.0};

float iMeasCal[] = {
    0.0,  // 0.0
    0.41, // 0.1
    0.58, // 0.2
    0.62, // 0.3
    0.72, // 0.4
    0.84, // 0.5
    0.82, // 0.6
    1.00, // 0.7
    1.07, // 0.8
    1.15, // 0.9
    1.23, // 1.0
    1.31, // 1.1
    1.37, // 1.2
    1.43, // 1.3
    1.51, // 1.4
    1.57, // 1.5
    1.64, // 1.6
    1.7,  // 1.7
    1.8,  // 1.8
    1.9,  // 1.9
    2.0   // 2.0
};

float vMeasCal[] = {
    -0.13,
    0.92,
    1.94,
    2.97,
    3.98,
    5.01,
    6.05,
    7.05,
    8.07,
    9.09,
    10.11,
    11.12,
    12.15,
    13.17,
    14.19,
    15.21,
    16.22,
    17.23,
    18.25,
    19.28,
    20.30,
    21.32,
    22.34,
    23.37,
    24.39,
    25.40,
    26.42,
    27.44,
    28.46,
    29.48,
    30.50,
};

float lerp(float y1, float y2, float dx, float y)
{
    return y * ((y2 - y1) / dx);
}

float toCalibrated(float raw, float table[], int n, float maxValue, float sign)
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
    float nearest = (float)idx * scale;
    float error = table[idx] - nearest;

    // Increase precision using linear interpolation (unless we're at the end of the table)
    if (idx < n - 1)
    {
        float next = (float)(idx + 1) * scale;
        float adj = lerp(table[idx] - nearest, table[idx + 1] - next, scale, raw - nearest);
        error += adj;
        /*
        Serial.print("raw=");
        Serial.print(raw);
        Serial.print(" nearest=");
        Serial.print(nearest);
        Serial.print(" next=");
        Serial.print(next);
        Serial.print(" adj=");
        Serial.print(adj);
        Serial.print(" error=");
        Serial.println(error);
        */
    }
    return fmax(0, raw + sign * error);
}

uint32_t toCalibratedVOutput(uint32_t v)
{
    return (uint32_t)(toCalibrated(((float)v) / 1000, vOutCal, sizeof(vOutCal) / sizeof(float), 30.0, -1) * 1000);
}

uint32_t toCalibratedIOutput(uint32_t i)
{
    return (uint32_t)(toCalibrated(((float)i) / 1000, iOutCal, sizeof(iOutCal) / sizeof(float), 2.0, -1) * 1000);
}

float toCalibratedIReading(float i)
{
    return toCalibrated(i / 1000, iMeasCal, sizeof(iMeasCal) / sizeof(float), 2.0, 1) * 1000;
}

float toCalibratedVReading(float v)
{
    return toCalibrated(v / 1000, vMeasCal, sizeof(vMeasCal) / sizeof(float), 30.0, 1) * 1000;
}
