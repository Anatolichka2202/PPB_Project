#ifndef COMMUNICATIONFACADE_H
#define COMMUNICATIONFACADE_H

#include "icommunication.h"
#include "communicationengine.h"
#include "udpclient.h"
#include "protocoladapter.h"

class CommunicationFacade : public ICommunication {
    Q_OBJECT
public:
    // Конструктор для внешнего UDPClient (владелец остаётся снаружи)
    explicit CommunicationFacade(UDPClient* udpClient,
                                 std::unique_ptr<IProtocolAdapter> adapter,
                                 QObject* parent = nullptr);

    // Конструктор, создающий UDPClient самостоятельно (удобно для простого использования)
    explicit CommunicationFacade(QObject* parent = nullptr);

    ~CommunicationFacade() override;

    // Инициализация (должна вызываться после перемещения в нужный поток)
    Q_INVOKABLE void initialize();

    // ICommunication interface
    bool connectToPPB(uint16_t address, const QString& ip, quint16 port) override;
    void disconnect() override;
    void executeCommand(TechCommand cmd, uint16_t address) override;
    void executeCommand(TechCommand cmd, uint16_t address, const QByteArray& data) override;
    void sendFUTransmit(uint16_t address) override;
    void sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3] = nullptr) override;
    void sendDataPackets(const QVector<DataPacket>& packets) override;

    PPBState state() const override;
    bool isBusy() const override;

    void setBridgeAddress(const QString &ip, quint16 port) override;

    Q_INVOKABLE quint64 executeGroupCommand(TechCommand cmd, uint16_t mask, const QByteArray& data = {});
signals:
    // Сигнал об успешной инициализации
    void initialized();
    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);
private slots:
    // Внутренние слоты для проброса сигналов от движка
    void onEngineStateChanged(uint16_t address, PPBState state);
    void onEngineCommandCompleted(bool success, const QString& message, TechCommand command);
    void onEngineErrorOccurred(const QString& error);
    void onEngineCommandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void onEngineSentPacketsSaved(const QVector<DataPacket>& packets);
    void onEngineReceivedPacketsSaved(const QVector<DataPacket>& packets);
    void onEngineClearPacketDataRequested();
    void onEngineCommandProgress(int current, int total, TechCommand command);
    void onEngineBusyChanged(bool busy);
    void onEngineGroupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);

private:
    // Вспомогательный метод для перенаправления вызовов в правильный поток
    template<typename Func>
    void invokeInThread(Func&& func);

    // Владеем ли мы UDPClient?
    bool m_ownsUdpClient;
    UDPClient* m_udpClient;
    communicationengine* m_engine;
};

#endif // COMMUNICATIONFACADE_H
