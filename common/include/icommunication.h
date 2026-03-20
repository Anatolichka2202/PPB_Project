#ifndef ICOMMUNICATION_H
#define ICOMMUNICATION_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QVariant>
#include "ppbprotocol.h"  // для TechCommand, PPBState, DataPacket

class ICommunication : public QObject {
    Q_OBJECT
public:
    explicit ICommunication(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ICommunication() = default;

    // Управление подключением
    virtual bool connectToPPB(uint16_t address, const QString& ip, quint16 port) = 0;
    virtual void disconnect() = 0;

    // Отправка команд
    virtual void executeCommand(TechCommand cmd, uint16_t address) = 0;
    virtual void sendFUTransmit(uint16_t address, uint8_t period, const uint8_t fuData[3] = nullptr) = 0;
    virtual void sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3] = nullptr) = 0;
    virtual void sendDataPackets(const QVector<DataPacket>& packets) = 0;
    virtual void executeCommand(TechCommand cmd, uint16_t address, const QByteArray& data) = 0;

    virtual PPBState state() const = 0;
    virtual bool isBusy() const = 0;

    virtual void setBridgeAddress(const QString &ip, quint16 port) = 0;
    virtual quint64 executeGroupCommand(TechCommand cmd, uint16_t mask, const QByteArray& data = {}) = 0;

signals:
    void stateChanged(uint16_t address, PPBState state);
    void connected();
    void disconnected();
    void commandCompleted(bool success, const QString& message, TechCommand command);
    void errorOccurred(const QString& error);
    void commandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void sentPacketsSaved(const QVector<DataPacket>& packets);
    void receivedPacketsSaved(const QVector<DataPacket>& packets);
    void clearPacketDataRequested();

    void statusReceived(uint16_t address, uint32_t mask, const QVector<QByteArray>& data);   // для TS
    void commandProgress(int current, int total, TechCommand command);        // прогресс отправки/приёма
    void busyChanged(bool busy);                                              // изменение занятости

    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);
    void fuCommandCompleted(uint16_t address, uint8_t command, bool success, const QString& message);
};

#endif // ICOMMUNICATION_H
