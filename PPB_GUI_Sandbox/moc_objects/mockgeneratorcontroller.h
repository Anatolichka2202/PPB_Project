#ifndef MOCKGENERATORCONTROLLER_H
#define MOCKGENERATORCONTROLLER_H

#include "iakipcontroller.h"
#include <QTimer>

class MockGeneratorController : public IAkipController
{
    Q_OBJECT

public:
    explicit MockGeneratorController(QObject *parent = nullptr);
    ~MockGeneratorController() override;

    // Управление подключением
    bool openDevice(int index = 0) override;
    void closeDevice() override;
    bool isOpen() const override;
    bool isAvailable() const override;

    // Высокоуровневые команды
    bool setFrequency(int channel, double freqHz) override;
    bool setOutput(int channel, bool enable) override;
    bool setAmplitude(int channel, double amplitude, const QString &unit = "VPP") override;
    bool setWaveform(int channel, const QString &waveform) override;
    bool setDutyCycle(int channel, double percent) override;
    bool setAMFrequency(int channel, double freqHz) override;
    bool setAMDepth(int channel, double percent) override;
    bool setAMState(int channel, bool enable) override;
    QString getIdentity() override;
    bool reset() override;

    // Запросы
    double queryFrequency(int channel) override;
    bool queryOutput(int channel) override;
    double queryAmplitude(int channel) override;
    QString queryWaveform(int channel) override;
    double queryDutyCycle(int channel) override;
    double queryAMFrequency(int channel) override;
    double queryAMDepth(int channel) override;
    bool queryAMState(int channel) override;

    // Низкоуровневые команды
    bool sendCommand(const QString &cmd) override;
    QString queryCommand(const QString &cmd) override;

    // Список поддерживаемых команд
    QStringList availableCommands() const override;

    // Методы с замером времени
    bool sendCommandTimed(const QString &cmd, qint64 &elapsedMs) override;
    bool queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs) override;


private:
    bool m_isOpen;
    bool m_available;
    int m_currentChannel;     // для эмуляции
    double m_freq;
    bool m_output;
    double m_ampl;
    double m_amFreq;
    double m_amDepth;
    bool m_amState;

};

#endif // MOCKGENERATORCONTROLLER_H
