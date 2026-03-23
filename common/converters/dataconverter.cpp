#include "dataconverter.h"
#include <cmath>
#include <QString>
#include <cstring>
#include <cstring>


uint32_t DataConverter::powerToCode(float watts)
{
    return ((uint32_t)(*((uint32_t *)& watts)))    ;
}

float DataConverter::codeToPower(uint32_t code)
{

    return ((float)(*((float *)& code)))    ;
}

int16_t DataConverter::temperatureToCode(float celsius)
{
    return ((uint32_t)(*((uint32_t *)& celsius)))    ;
}

float DataConverter::codeToTemperature(int16_t code)
{
    return ((uint32_t)(*((uint32_t *)& code)))    ;
}

uint16_t DataConverter::vswrToCode(float vswr)
{
    return ((uint32_t)(*((uint32_t *)& vswr)))    ;
}

float DataConverter::codeToVSWR(uint16_t code)
{
    return ((uint32_t)(*((uint32_t *)& code)))    ;
}




QString DataConverter::formatPower(float watts, bool withUnit)
{
    if (withUnit) {
        return QString("%1 Вт").arg(watts, 0, 'f', 1);
    }
    return QString::number(watts, 'f', 1);
}

QString DataConverter::formatTemperature(float celsius, bool withUnit)
{
    if (withUnit) {
        return QString("%1 °C").arg(celsius, 0, 'f', 1);
    }
    return QString::number(celsius, 'f', 1);
}

QString DataConverter::formatVSWR(float vswr)
{
    return QString::number(vswr, 'f', 2);
}

QString DataConverter::formatDuration(float microseconds, bool withUnit)
{
    if (withUnit) {
        return QString("%1 мкс").arg(microseconds, 0, 'f', 0);
    }
    return QString::number(microseconds, 'f', 0);
}
