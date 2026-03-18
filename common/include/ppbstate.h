#ifndef PPBSTATE_H
#define PPBSTATE_H

#include <cstdint>
#include <QString>

/**
 * @brief Реальное состояние ППБ, полученное из команд TS, VERS, CHECKSUM и др.
 *
 * Содержит все данные, приходящие от ППБ. Поля с суффиксом Code хранят исходные
 * коды из протокола, остальные — физические величины. Маска валидации (validationMask)
 * показывает, какие блоки данных актуальны в последнем ответе TS.
 */
struct PPBFullState {
    // === Идентификация ===
    uint16_t addressMask = 0;      // битовый адрес (1 << index)

    // === Маска валидации (из TS) ===
    uint32_t validationMask = 0;   // 24-битная маска, указывающая на свежесть блоков

    // === Температуры (из TS) ===
    int16_t tempT1Code = 0, tempT2Code = 0, tempT3Code = 0, tempT4Code = 0;
    int16_t tempV1Code = 0, tempV2Code = 0;
    int16_t tempOutCode = 0, tempInCode = 0;
    float tempT1 = 0.0f, tempT2 = 0.0f, tempT3 = 0.0f, tempT4 = 0.0f;
    float tempV1 = 0.0f, tempV2 = 0.0f;
    float tempOut = 0.0f, tempIn = 0.0f;

    // === Канал 1 (из TS) ===
    struct {
        uint32_t powerCode = 0;    // код мощности
        uint16_t vswrCode = 0;     // код КСВН
        float power = 0.0f;         // мощность, Вт
        float vswr = 1.0f;          // КСВН
        bool isOk = false;           // исправность канала (из бита 0 маски состояния)
    } ch1;

    // === Канал 2 (из TS) ===
    struct {
        uint32_t powerCode = 0;
        uint16_t vswrCode = 0;
        float power = 0.0f;
        float vswr = 1.0f;
        bool isOk = false;
    } ch2;

    // === Результаты других команд ===
    QString version;                // версия прошивки (из VERS)
    uint16_t checksum = 0;          // контрольная сумма (из CHECKSUM)
    uint16_t droppedPackets = 0;    // отброшенные пакеты ФУ (из DROP)

    // Коэффициенты ошибок
    uint32_t berErrorsTU = 0;       // количество ошибок линии ТУ (из BER_T)
    uint32_t berErrorsFU = 0;       // количество ошибок линии ФУ (из BER_F)
    float berTU = 0.0f;             // коэффициент ошибок ТУ
    float berFU = 0.0f;             // коэффициент ошибок ФУ

    uint16_t factoryNumber = 0;     // заводской номер (из Factory_Number)

    // Сброс в значения по умолчанию
    void clear() { *this = PPBFullState(); }
};

/**
 * @brief Настройки ППБ, которые редактирует пользователь и отправляет в TC.
 *
 * Эти значения не обновляются автоматически из TS. Они используются для
 * формирования команды TC и отображаются в полях ввода виджетов.
 */
struct PpbUserSettings {
    // Мощности каналов (физические величины, Вт)
    float power1 = 0.0f;
    float power2 = 0.0f;

    // Флаги состояния (8-битная маска, отправляемая в TC)
    bool fuBlocked = false;        // бит 1 – блокировка ФУ
    bool rebootRequested = false;  // бит 2 – запрос перезагрузки
    bool resetErrors = false;      // бит 3 – сброс ошибок
    bool powerEnabled = false;     // бит 8 – включение питания
};

#endif // PPBSTATE_H
