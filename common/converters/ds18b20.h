#ifndef DS18B20_H
#define DS18B20_H

#include <cstdint>

class ds18b20
{
public:
    ds18b20();
    static float convert(int16_t dtg);
    static uint8_t getSign(int16_t dtg);
};

#endif // DS18B20_H
