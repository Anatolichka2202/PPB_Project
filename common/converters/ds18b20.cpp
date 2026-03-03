#include "ds18b20.h"
#include <cstdint>

ds18b20::ds18b20() {}

float ds18b20::convert(int16_t dtg) {
    float t;
    t = (float)((dtg & 0x07FF) >> 4);
    t += (float)(dtg & 0x000F) / 16.0f;
    return t;
}

uint8_t ds18b20::getSign(int16_t dtg) {
    return (dtg & (1 << 11)) ? 1 : 0;
}
