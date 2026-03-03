#ifndef DATACONVERTER_H
#define DATACONVERTER_H

#include <cstdint>
#include <QString>

//тест преобразования
class DataConverter
{
public:
    // === ПРЕОБРАЗОВАНИЕ МОЩНОСТИ ===
    // Диапазон: 0-2000 Вт
    static uint16_t powerToCode(float watts);
    static float codeToPower(uint16_t code);

    // === ПРЕОБРАЗОВАНИЕ ТЕМПЕРАТУРЫ ===
    // Диапазон: -55..125 °C,
    static int16_t temperatureToCode(float celsius);
    static float codeToTemperature(int16_t code);

    // === ПРЕОБРАЗОВАНИЕ КСВН ===
    // Диапазон: 1.0-10.0
    static uint16_t vswrToCode(float vswr);
    static float codeToVSWR(uint16_t code);

    // === ПРЕОБРАЗОВАНИЕ ДЛИТЕЛЬНОСТИ ИМПУЛЬСА ===
    // Диапазон: 0-100000 мкс
    static uint32_t durationToCode(float microseconds);
    static float codeToDuration(uint32_t code);

    // === ПРЕОБРАЗОВАНИЕ СКВАЖНОСТИ ===
    // Диапазон: 1-255
    static uint8_t dutyCycleToCode(float duty);
    static float codeToDutyCycle(uint8_t code);

    // === УТИЛИТЫ ===
    static QString formatPower(float watts, bool withUnit = true);
    static QString formatTemperature(float celsius, bool withUnit = true);
    static QString formatVSWR(float vswr);
    static QString formatDuration(float microseconds, bool withUnit = true);

    // === ЗАГЛУШКИ (пока возвращаем то же самое) ===
    static float dummyConvert(float value) { return value; }
    static uint16_t dummyConvert(uint16_t value) { return value; }
};

#endif // DATACONVERTER_H
