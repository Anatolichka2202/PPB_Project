#ifndef IAKIPCONTROLLER_H
#define IAKIPCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

class IAkipController : public QObject
{
    Q_OBJECT

public:
    explicit IAkipController(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IAkipController() {}

    // ==================== Управление подключением ====================
    virtual bool openDevice(int index = 0) = 0;
    virtual void closeDevice() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isAvailable() const = 0;

    // ==================== Высокоуровневые команды ====================
    virtual bool setFrequency(int channel, double freqHz) = 0;
    virtual bool setOutput(int channel, bool enable) = 0;
    virtual bool setAmplitude(int channel, double amplitude, const QString &unit = "VPP") = 0;
    virtual bool setWaveform(int channel, const QString &waveform) = 0;
    virtual bool setDutyCycle(int channel, double percent) = 0;          // NEW
    virtual bool setAMFrequency(int channel, double freqHz) = 0;        // NEW
    virtual bool setAMDepth(int channel, double percent) = 0;           // NEW
    virtual bool setAMState(int channel, bool enable) = 0;              // NEW
    virtual QString getIdentity() = 0;
    virtual bool reset() = 0;

    // ==================== Запросы текущих значений ====================
    virtual double queryFrequency(int channel) = 0;
    virtual bool queryOutput(int channel) = 0;
    virtual double queryAmplitude(int channel) = 0;
    virtual QString queryWaveform(int channel) = 0;
    virtual double queryDutyCycle(int channel) = 0;                      // NEW
    virtual double queryAMFrequency(int channel) = 0;                    // NEW
    virtual double queryAMDepth(int channel) = 0;                        // NEW
    virtual bool queryAMState(int channel) = 0;                          // NEW

    // ==================== Низкоуровневые команды ====================
    virtual bool sendCommand(const QString &cmd) = 0;
    virtual QString queryCommand(const QString &cmd) = 0;

    // ==================== Список поддерживаемых команд ====================
    virtual QStringList availableCommands() const = 0;

signals:
    void opened();
    void closed();
    void availabilityChanged(bool available);
    void errorOccurred(const QString &error);
    void frequencyChanged(int channel, double freq);
    void outputChanged(int channel, bool enabled);
    void amplitudeChanged(int channel, double amplitude);
    void waveformChanged(int channel, const QString &waveform);
    void dutyCycleChanged(int channel, double percent);                  // NEW
    void amFrequencyChanged(int channel, double freq);                   // NEW
    void amDepthChanged(int channel, double percent);                    // NEW
    void amStateChanged(int channel, bool enabled);                      // NEW

    // ==================== Методы с замером времени ====================
    public:
    virtual bool sendCommandTimed(const QString &cmd, qint64 &elapsedMs) = 0;
    virtual bool queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs) = 0;
};

#endif // IAKIPCONTROLLER_H
