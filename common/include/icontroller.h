#ifndef ICONTROLLER_H
#define ICONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QVariantMap>
#include "ppbprotocol.h"      // для PPBState, TechCommand, DataPacket
#include "ppbstate.h"         // для PPBFullState, PpbUserSettings

struct TCDataPayload; // forward, определён в ppbprotocol.h

class IController : public QObject {
    Q_OBJECT
public:
    explicit IController(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IController() = default;

    // === Управление подключением ===
    virtual void connectToPPB(uint16_t address, const QString& ip, quint16 port) = 0;
    virtual void disconnect() = 0;
    virtual void setBridgeAddress(const QString &ip, quint16 port) = 0;

    // === Основные команды ===
    virtual void requestStatus(uint16_t address) = 0;
    virtual void sendTC(uint16_t address) = 0;                      // отправляет тех.управление с текущими настройками
    virtual void resetPPB(uint16_t address, const TCDataPayload& payload) = 0; // устаревший? можно удалить

    // === Команды ФУ ===
    virtual void setFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle) = 0;
    virtual void setFUTransmit(uint16_t address, uint16_t duration, uint16_t dutyCycle) = 0;
    virtual void startPRBS_M2S(uint16_t address) = 0;
    virtual void startPRBS_S2M(uint16_t address) = 0;

    // === Информационные команды ===
    virtual void requestVersion(uint16_t address) = 0;
    virtual void requestVolume(uint16_t address) = 0;
    virtual void requestChecksum(uint16_t address) = 0;
    virtual void sendProgram(uint16_t address) = 0;
    virtual void sendClean(uint16_t address) = 0;
    virtual void requestDroppedPackets(uint16_t address) = 0;
    virtual void requestBER_T(uint16_t address) = 0;
    virtual void requestBER_F(uint16_t address) = 0;
    virtual void requestFabricNumber(uint16_t address) = 0;

    // === Анализ пакетов ===
    virtual void analize() = 0;
    virtual void saveReceivedPackets(const QVector<DataPacket>& packets) = 0;
    virtual void saveSentPackets(const QVector<DataPacket>& packets) = 0;

    // === Автоопрос ===
    virtual void startAutoPoll(int intervalMs = 5000) = 0;
    virtual void stopAutoPoll() = 0;
    virtual bool isAutoPollEnabled() const = 0;

    // === Состояние и занятость ===
    virtual PPBState connectionState() const = 0;
    virtual bool isBusy() const = 0;
    virtual void setCurrentAddress(uint16_t address) = 0;

    // === Получение реального состояния ППБ ===
    virtual PPBFullState getFullState(uint8_t ppbIndex) const = 0;

    // === Работа с настройками пользователя (для TC) ===
    virtual const PpbUserSettings& getUserSettings(uint8_t ppbIndex) const = 0;
    virtual void setPowerSetting(uint8_t ppbIndex, int channel, float watts) = 0;
    virtual void setFuBlockedSetting(uint8_t ppbIndex, bool blocked) = 0;
    virtual void setRebootRequestedSetting(uint8_t ppbIndex, bool reboot) = 0;
    virtual void setResetErrorsSetting(uint8_t ppbIndex, bool reset) = 0;
    virtual void setPowerEnabledSetting(uint8_t ppbIndex, bool enabled) = 0;
    virtual void resetSettingsToReal(uint8_t ppbIndex) = 0;

    // === Низкоуровневый доступ к коммуникации (опционально) ===
    virtual void setCommunication(class ICommunication* communication) = 0;
    virtual void exucuteCommand(TechCommand tech, uint16_t address) = 0; // прямая отправка

    virtual void runFullTest(uint16_t address) =0;
signals:
    // Состояние подключения и занятость
    void connectionStateChanged(PPBState state);
    void busyChanged(bool busy);

    // Обновление реального состояния ППБ
    void fullStateUpdated(uint8_t ppbIndex);

    // Обновление настроек пользователя
    void userSettingsChanged(uint8_t ppbIndex);

    // Результаты команд
    void statusReceived(uint16_t address, const QVector<QByteArray>& data);
    void commandCompleted(bool success, const QString& message, TechCommand command);
    void commandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);
    void errorOccurred(const QString& error);

    // Прогресс операций
    void operationProgress(int current, int total, const QString& operation);
    void operationCompleted(bool success, const QString& message);

    // Анализ пакетов
    void analysisStarted();
    void analysisProgress(int percent);
    void analysisComplete(const QString& summary, const QVariantMap& details);

    // Доступность АКИП
    void akipAvailabilityChanged(bool available);
};

#endif // ICONTROLLER_H
