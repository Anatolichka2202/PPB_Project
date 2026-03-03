#ifndef ICONTROLLER_H
#define ICONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QVariantMap>
#include "ppbprotocol.h"  // для PPBState, TechCommand и DataPacket

// Структура состояния канала для UI
struct UIChannelState {
    float power;          // Мощность, Вт
    float temperature;    // Основная температура, °C (для канала 1 – t1, для канала 2 – t2)
    float vswr;           // КСВН
    bool isOk;            // Флаг исправности

    // Новые поля (общие для ППБ)
    float tempT1;         // температура t1 (блок питания)
    float tempT2;         // температура t2 (блок питания)
    float tempT3;         // температура t3 (блок питания)
    float tempT4;         // температура t4 (блок питания)
    float tempV1;         // температура v1 ()
    float tempV2;         // температура v2 ()
    float tempOut;        // температура выходных резисторов
    float tempIn;         // температура входных резисторов
};
Q_DECLARE_METATYPE(UIChannelState)  // для использования в сигналах/слотах
class IController : public QObject {
    Q_OBJECT
public:
    explicit IController(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IController() = default;

    // API для GUI
    virtual void connectToPPB(uint16_t address, const QString& ip, quint16 port) = 0;
    virtual void disconnect() = 0;
    virtual void requestStatus(uint16_t address) = 0;
    virtual void resetPPB(uint16_t address) = 0;
    virtual void setGeneratorParameters(uint16_t address, uint32_t duration, uint8_t duty, uint32_t delay) = 0;
    virtual void setFUReceive(uint16_t address, uint8_t period = 0) = 0;
    virtual void setFUTransmit(uint16_t address) = 0;
    virtual void startPRBS_M2S(uint16_t address) = 0;
    virtual void startPRBS_S2M(uint16_t address) = 0;
    virtual void runFullTest(uint16_t address) = 0;
    virtual void startAutoPoll(int intervalMs = 5000) = 0;
    virtual void stopAutoPoll() = 0;
    virtual void setCurrentAddress(uint16_t address) = 0;
    virtual PPBState connectionState() const = 0;
    virtual bool isBusy() const = 0;
    virtual bool isAutoPollEnabled() const = 0;
    virtual UIChannelState getChannelState(uint8_t ppbIndex, int channel) const = 0;
    virtual void requestVersion(uint16_t address) = 0;
    virtual void requestVolume(uint16_t address) = 0;
    virtual void requestChecksum(uint16_t address) = 0;
    virtual void sendProgram(uint16_t address) = 0;
    virtual void sendClean(uint16_t address) = 0;
    virtual void requestDroppedPackets(uint16_t address) = 0;
    virtual void requestBER_T(uint16_t address) = 0;
    virtual void requestBER_F(uint16_t address) = 0;
    virtual void analize() = 0;
    virtual void saveReceivedPackets(const QVector<DataPacket>& packets) = 0;
    virtual void saveSentPackets(const QVector<DataPacket>& packets) = 0;
    virtual void setCommunication(class ICommunication* communication) = 0;

signals:
    void connectionStateChanged(PPBState state);
    void busyChanged(bool busy);
    void statusReceived(uint16_t address, const QVector<QByteArray>& data);
    void errorOccurred(const QString& error);
    void channelStateUpdated(uint8_t ppbIndex, int channel, const UIChannelState& state);
    void autoPollToggled(bool enabled);
    void operationProgress(int current, int total, const QString& operation);
    void operationCompleted(bool success, const QString& message);
    void analysisStarted();
    void analysisProgress(int percent);
    void analysisComplete(const QString& summary, const QVariantMap& details);
};

#endif // ICONTROLLER_H
