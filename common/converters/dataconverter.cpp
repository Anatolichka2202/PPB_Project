#include "dataconverter.h"
#include <cmath>
#include <QString>
#include <cstring>
#include <cstring>

// === ЗАГЛУШКИ - ПОКА ВОЗВРАЩАЕМ ТО ЖЕ САМОЕ ===

uint32_t DataConverter::powerToCode(float watts)
{
    uint32_t integer = static_cast<uint32_t>(watts);
    float frac = watts - integer;
    if (frac < 0) frac = 0;
    uint32_t fractional = static_cast<uint32_t>(frac * 65536.0f + 0.5f);
    if (fractional > 0xFFFF) fractional = 0xFFFF;
    return (integer << 16) | (fractional & 0xFFFF);
}

float DataConverter::codeToPower(uint32_t code)
{
    uint32_t integer = code >> 16;
    uint32_t fractional = code & 0xFFFF;
    return integer + fractional / 65536.0f;
}

int16_t DataConverter::temperatureToCode(float celsius)
{
    // Заглушка: 1 °C = 1 код
    return static_cast<int16_t>(celsius);
}

float DataConverter::codeToTemperature(int16_t code)
{
    // Заглушка: 1 код = 1 °C
    return static_cast<float>(code);
}

uint16_t DataConverter::vswrToCode(float vswr)
{
    // Заглушка: 1.0 = 100
    return static_cast<uint16_t>(vswr * 100.0f);
}

float DataConverter::codeToVSWR(uint16_t code)
{
    // Заглушка: 100 = 1.0
    return static_cast<float>(code) / 100.0f;
}

uint32_t DataConverter::durationToCode(float microseconds)
{
    // Заглушка: 1 мкс = 1 код
    return static_cast<uint32_t>(microseconds);
}

float DataConverter::codeToDuration(uint32_t code)
{
    // Заглушка: 1 код = 1 мкс
    return static_cast<float>(code);
}

uint8_t DataConverter::dutyCycleToCode(float duty)
{
    // Заглушка
    return static_cast<uint8_t>(duty);
}

float DataConverter::codeToDutyCycle(uint8_t code)
{
    // Заглушка
    return static_cast<float>(code);
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
