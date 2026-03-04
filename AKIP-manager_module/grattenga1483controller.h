#ifndef GRATTENGA1483CONTROLLER_H
#define GRATTENGA1483CONTROLLER_H

#include "abstractscpicontroller.h"
#include "lanInterface.h"

class GrattenGa1483Controller : public AbstractScpiController
{
    Q_OBJECT
public:
    explicit GrattenGa1483Controller(QObject *parent = nullptr);
    ~GrattenGa1483Controller() override;

    // Дополнительный метод для установки параметров подключения (вызывать перед openDevice)
    void setConnectionParameters(const QString &host, quint16 port = 5025);

    // ==================== Управление подключением ====================
    bool openDevice(int index = 0) override;   // index игнорируется
    void closeDevice() override;
    bool isOpen() const override;
    bool isAvailable() const override;

    // ==================== Высокоуровневые команды ====================
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

    // ==================== Запросы текущих значений ====================
    double queryFrequency(int channel) override;
    bool queryOutput(int channel) override;
    double queryAmplitude(int channel) override;
    QString queryWaveform(int channel) override;
    double queryDutyCycle(int channel) override;
    double queryAMFrequency(int channel) override;
    double queryAMDepth(int channel) override;
    bool queryAMState(int channel) override;

    // ==================== Низкоуровневые команды ====================
    bool sendCommand(const QString &cmd) override;
    QString queryCommand(const QString &cmd) override;

    // ==================== Список поддерживаемых команд ====================
    QStringList availableCommands() const override;

    // ==================== Чисто виртуальные методы AbstractScpiController ===============
    bool isTransportAvailable() const override;
    QString querySystemError() override;

private slots:
    void onConnected();
    void onDisconnected();
    void onError(const QString &error);

private:
    LanInterface m_lan;
    bool m_available;        // флаг, что устройство отвечает (после *IDN?)
    QString m_host;
    quint16 m_port;

    // Вспомогательные методы
    QString unitToGratten(const QString &unit) const; // перевод единиц (сохранён для совместимости, но не используется)
};

#endif // GRATTENGA1483CONTROLLER_H
