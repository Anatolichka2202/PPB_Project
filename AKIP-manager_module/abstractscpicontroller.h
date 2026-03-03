#ifndef ABSTRACTSCPICONTROLLER_H
#define ABSTRACTSCPICONTROLLER_H

#include "iakipcontroller.h"
#include <QMap>
#include <functional>

class AbstractScpiController : public IAkipController
{
    Q_OBJECT
public:
    explicit AbstractScpiController(QObject *parent = nullptr);
    virtual ~AbstractScpiController();

    // ==================== Управление подключением ====================
    virtual bool openDevice(int index = 0) override = 0;
    virtual void closeDevice() override = 0;
    virtual bool isOpen() const override = 0;
    virtual bool isAvailable() const override = 0;

    // ==================== Высокоуровневые команды ====================
    virtual bool setFrequency(int channel, double freqHz) override = 0;
    virtual bool setOutput(int channel, bool enable) override = 0;
    virtual bool setAmplitude(int channel, double amplitude, const QString &unit = "VPP") override = 0;
    virtual bool setWaveform(int channel, const QString &waveform) override = 0;
    virtual bool setDutyCycle(int channel, double percent) override = 0;
    virtual bool setAMFrequency(int channel, double freqHz) override = 0;
    virtual bool setAMDepth(int channel, double percent) override = 0;
    virtual bool setAMState(int channel, bool enable) override = 0;
    virtual QString getIdentity() override = 0;
    virtual bool reset() override = 0;

    // ==================== Запросы текущих значений ====================
    virtual double queryFrequency(int channel) override = 0;
    virtual bool queryOutput(int channel) override = 0;
    virtual double queryAmplitude(int channel) override = 0;
    virtual QString queryWaveform(int channel) override = 0;
    virtual double queryDutyCycle(int channel) override = 0;
    virtual double queryAMFrequency(int channel) override = 0;
    virtual double queryAMDepth(int channel) override = 0;
    virtual bool queryAMState(int channel) override = 0;

    // ==================== Низкоуровневые команды ====================
    virtual bool sendCommand(const QString &cmd) override = 0;
    virtual QString queryCommand(const QString &cmd) override = 0;

    // ==================== Список поддерживаемых команд ====================
    virtual QStringList availableCommands() const override = 0;

    // ==================== Методы с замером времени ====================
    virtual bool sendCommandTimed(const QString &cmd, qint64 &elapsedMs) override final;
    virtual bool queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs) override final;

protected:
    // Кэши для хранения текущих значений
    QMap<int, double> m_freqCache;
    QMap<int, bool> m_outputCache;
    QMap<int, double> m_amplCache;
    QMap<int, QString> m_waveCache;
    QMap<int, double> m_amFreqCache;
    QMap<int, double> m_amDepthCache;
    QMap<int, bool> m_amStateCache;
    QMap<int, double> m_dutyCycleCache;

    // Проверка доступности устройства
    bool ensureAvailable() const;
    virtual bool isTransportAvailable() const = 0;

    // Запрос системной ошибки (должен быть реализован в наследнике)
    virtual QString querySystemError() = 0;

    // ==================== Верификация команд ====================
    // Для числовых значений
    bool verifySetCommand(const QString &cmd,
                          std::function<double()> queryFunc,
                          double expectedValue,
                          double tolerance = 0.01,
                          int maxRetries = 2,
                          int delayMs = 100);

    // Для булевых значений
    bool verifySetCommand(const QString &cmd,
                          std::function<bool()> queryFunc,
                          bool expectedValue,
                          int maxRetries = 2,
                          int delayMs = 100);

    // ==================== Словарь команд ====================
    struct CommandDef
    {
        QString setPattern;   // строка с %1 для подстановки значения
        QString queryCmd;     // команда запроса (без %1)
        double minVal;        // минимальное значение (для числовых)
        double maxVal;        // максимальное значение
        QString unit;         // единица измерения (для информации)
        bool isBoolean;       // true для булевых команд (тогда min/max игнорируются)
    };

    // Регистрация команды в словаре
    void registerCommand(const QString &key, const CommandDef &def);

    // Проверка диапазона для числовой команды
    bool checkRange(const QString &key, double val) const;

    // Формирование команды установки с подстановкой значения
    QString buildSetCommand(const QString &key, double val) const;

    // Формирование команды установки для булевых значений
    QString buildSetCommand(const QString &key, bool val) const;

    // Получение команды запроса
    QString buildQueryCommand(const QString &key) const;

private:
    QMap<QString, CommandDef> m_commandMap;
};

#endif // ABSTRACTSCPICONTROLLER_H
