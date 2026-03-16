#ifndef PPBSTATE_H
#define PPBSTATE_H

#include <cstdint>
#include <QString>

// Полное состояние одного ППБ (все параметры, полученные из TS и других команд)
struct PPBFullState {
    uint16_t addressMask = 0;   // битовый адрес (1 << index)
    uint32_t statusMask = 0;    // маска, полученная в последнем TS

    // Флаги управления (из TC или состояния)
    bool fuBlocked = false;     // блокировка ФУ            бит 1
    bool rebootRequested = false;   // бит перезагрузки     бит 2
    bool resetErrors = false; // бит сброса ошибок          бит 3
    bool powerEnabled = false;   // бит 8 (0x80) в TC       бит 8

    // Температуры (общие для ППБ)
    float tempT1 = 0.0f, tempT2 = 0.0f, tempT3 = 0.0f, tempT4 = 0.0f;
    float tempV1 = 0.0f, tempV2 = 0.0f;
    float tempOut = 0.0f, tempIn = 0.0f;

    // Канал 1
    struct {
        float power = 0.0f;     // Вт
        float vswr = 1.0f;
        bool isOk = false;
    } ch1;

    // Канал 2
    struct {
        float power = 0.0f;
        float vswr = 1.0f;
        bool isOk = false;
    } ch2;

    // Результаты других команд (могут заполняться отдельно)
    QString version;
    uint16_t checksum = 0;
    uint16_t droppedPackets = 0;
    uint32_t berErrors = 0;
    float ber = 0.0f;
    uint16_t factoryNumber = 0;

    // Метод для сброса в значения по умолчанию
    void clear() {
        *this = PPBFullState();
    }
};

#endif // PPBSTATE_H
