#ifndef COMMUNICATIONENGINE_H
#define COMMUNICATIONENGINE_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <deque>
#include <list>
#include <memory>
#include <QVariant>
#include "udpclient.h"
#include "packetbuilder.h"
#include "commandandoperation.h"
#include "iprotocol_adapter.h"


namespace Internal {
class StateManager : public QObject {       //управляет состоянием для каждого адреса
    Q_OBJECT



public:
    explicit StateManager(QObject* parent = nullptr);
    PPBState getState(uint16_t address) const;
    void setState(uint16_t address, PPBState state);


    void clear();

    QMap<uint16_t, PPBState> states() const {
        QMutexLocker locker(&m_mutex);
        return m_states;
    }

signals:
    void stateChanged(uint16_t address, PPBState state);

private:
    mutable QMutex m_mutex;
    QMap<uint16_t, PPBState> m_states;
};

class CommandQueue : public QObject {           // управляет очередями комад для каждого адреса
    Q_OBJECT
public:
    explicit CommandQueue(QObject* parent = nullptr);
    ~CommandQueue();

    void enqueue(uint16_t address, std::unique_ptr<PPBCommand> cmd);
    std::unique_ptr<PPBCommand> dequeue(uint16_t address);
    bool isEmpty(uint16_t address) const;
    void clear();



    // Получить список всех адресов, для которых есть очереди
    QList<uint16_t> addresses() const;

signals:
    void queueChanged(uint16_t address, int size);

private:
    mutable QMutex m_mutex;

    // Используем std::unordered_map
    std::unordered_map<uint16_t, std::deque<std::unique_ptr<PPBCommand>>> m_queues;
};

} // namespace Internal, хранение и управление очередями и состояниями по каждому адресу, используется движком



class communicationengine : public QObject
{
    Q_OBJECT

private:

    struct PPBContext {
        std::unique_ptr<PPBCommand> currentCommand;
        QVector<DataPacket> generatedPackets;
        QVector<DataPacket> receivedPackets;
        bool waitingForOk = false;
        bool operationCompleted = false;
        // В PPBContext
        std::unique_ptr<QTimer> operationTimer;
        //результаты парсинга от командды

        QString parsedMessage;           // Сообщение от команды
        bool parsedSuccess = false;      // Успешен ли парсинг
        QVariant parsedData;             // Дополнительные распарсенные данные
        QVector<DataPacket> parsedPackets; // Для команд с пакетами (PRBS_S2M)

        PPBState stateBeforeCommand = PPBState::Idle;  // Состояние перед началом команды
        bool suppressSend = false; //флаг мультивещания для создания контекстов
        // Конструкторы и операторы
        PPBContext() = default;
        PPBContext(const PPBContext&) = delete;
        PPBContext& operator=(const PPBContext&) = delete;



        //конструткор перемещения
        PPBContext(PPBContext&& other) noexcept
            : currentCommand(std::move(other.currentCommand))
            , generatedPackets(std::move(other.generatedPackets))
            , receivedPackets(std::move(other.receivedPackets))
            , waitingForOk(other.waitingForOk)
            , operationTimer(std::move(other.operationTimer))
        {
            // other.operationTimer  автоматически стал nullptr после std::move
        }

        //оператор присваивания
        PPBContext& operator=(PPBContext&& other) noexcept {
            if (this != &other) {


                currentCommand = std::move(other.currentCommand);
                generatedPackets = std::move(other.generatedPackets);
                receivedPackets = std::move(other.receivedPackets);
                waitingForOk = other.waitingForOk;
                if (operationTimer) operationTimer->stop();
                operationTimer = std::move(other.operationTimer);
            }
            return *this;
        }

        ~PPBContext() {
            if (operationTimer) operationTimer->stop();
        }

        // Метод для сброса результатов парсинга
        void clearParseResults() {
            parsedMessage.clear();
            parsedSuccess = false;
            parsedData.clear();
            parsedPackets.clear();
        }
    };

    //++++++++++++++++++++++++++++++++++++информация группового контекста+++++++++++++++++++++++//
    struct GroupInfo {
        QSet<uint16_t> pendingAddresses;   // адреса, ещё не ответившие
        QMap<uint16_t, bool> results;      // результаты по каждому адресу (успех/ошибка)
        QMap<uint16_t, QString> messages;  // сообщения для каждого адреса
        // Можно добавить общий таймер группы, если нужен
    };
    std::map<quint64, GroupInfo> m_groupOperations;
    quint64 m_nextGroupId = 1;
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

    class DataCommand : public PPBCommand {
    public:
        DataCommand(TechCommand cmd, const QByteArray& data)
            : m_cmd(cmd), m_data(data) {}
        TechCommand commandId() const override { return m_cmd; }
        QString name() const override { return CommandFactory::commandName(m_cmd); }
        QByteArray buildRequest(uint16_t address) const override {
            QByteArray base = PacketBuilder::createTURequest(address, m_cmd);
            QByteArray payload = m_data;
            if (payload.size() < 12) payload.resize(12, 0); // для TC всегда 12 байт
            return base + payload;
        }
        int expectedResponsePackets() const override { return 0; } // TC не возвращает данные
        int timeoutMs() const override { return PPBConstants::OPERATION_TIMEOUT_MS; }
    private:
        TechCommand m_cmd;
        QByteArray m_data;
    };

public:
    void completeOperation(uint16_t address, bool success, const QString& message);  // Универсальное завершение операции (успешное или с ошибкой)
    explicit communicationengine(UDPClient* udpClient, std::unique_ptr<IProtocolAdapter> adapter, QObject* parent = nullptr);
    ~communicationengine();

    void setCommandInterface(CommandInterface* cmdInterface) {
        m_commandInterface = cmdInterface;
    }

    PPBState overallState() const;
    bool isBusy() const { return m_busy; }

    void setBridgeAddress(const QString &ip, quint16 port);


public slots:
    // Основные методы
    bool connectToPPB(uint16_t address, const QString& ip, quint16 port);
    void disconnect();
    void executeCommand(TechCommand cmd, uint16_t address);
    void executeCommand(TechCommand cmd, uint16_t address, const QByteArray& data);


    // ФУ команды
    void sendFUTransmit(uint16_t address);
    void sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3] = nullptr);


    void sendPacketInternal(const QByteArray& packet, const QString& description);

    void setCommandParseResult(uint16_t address, bool success, const QString& message);
    void setCommandParseData(uint16_t address, const QVariant& data);

    void notifySentPackets(const QVector<DataPacket>& packets);
    void notifyReceivedPackets(const QVector<DataPacket>& packets);
    void requestClearPacketData();
    void sendDataPackets(const QVector<DataPacket>& packets);

    quint64 executeGroupCommand(TechCommand cmd, uint16_t mask, const QByteArray& data = {});

signals:
    void stateChanged(uint16_t address, PPBState state);
    void connected();
    void disconnected();
    void commandCompleted(bool success, const QString& report, TechCommand command);

    void errorOccurred(const QString& error);
   // void logMessage(const QString& message);

    void statusDataReady(const QVector<QByteArray>& data);
    void testDataReady(const QVector<DataPacket>& data);
    void commandProgress(int current, int total, TechCommand command);

    void commandDataParsed(uint16_t address, const QVariant& data, TechCommand command);

    void sentPacketsSaved(const QVector<DataPacket>& packets);
    void receivedPacketsSaved(const QVector<DataPacket>& packets);
    void clearPacketDataRequested();

    void statusReceived(uint16_t address, uint16_t mask, const QVector<QByteArray>& data);
    void busyChanged(bool busy);

    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);

    void fuCommandCompleted(uint16_t address, bool success, const QString& message);

private slots:
    void onDataReceived(const QByteArray& data, const QHostAddress& sender, quint16 port);
    void onNetworkError(const QString& error);
    void onOperationTimeout(uint16_t address);
    void processCommandQueue();
    void sendFUReceiveImpl(uint16_t address, uint8_t period, const QByteArray& fuData = QByteArray());
private:
    void executeCommandImmediately(uint16_t address, std::unique_ptr<PPBCommand> command, bool suppressSend = false);
    void clearContext(uint16_t address);
    //PPBContext* getContext(uint16_t address);

    //методы: поиск/геттер-private
    PPBContext* findContext(uint16_t address);
    PPBContext& getOrCreateContext(uint16_t address);

    //машина состояний
    void transitionState(uint16_t addres, PPBState newState, const QString& reason); //явная смена состояния
    QString stateToString(PPBState state) const;// Вспомогательная функция для логирования состояний
    void processNextCommandForAddress(uint16_t address); // Обработка следующей команды для указанного адреса

    bool canExecuteCommand(uint16_t address, const PPBCommand* command) const;

    void updateBusyState();

    QString errorCodeToString(uint8_t code) const;
private:

    UDPClient* m_udpClient;
    QTimer* m_queueTimer;
    std::unordered_map<uint16_t, PPBContext> m_contexts;
    std::unique_ptr<IProtocolAdapter> m_protocolAdapter;


    mutable QMutex m_contextsMutex;

    uint16_t m_currentAddress;
    QString m_currentIP;
    quint16 m_currentPort;
    mutable QMutex m_mutex;
    Internal::StateManager* m_stateManager;
    Internal::CommandQueue* m_commandQueue;
    CommandInterface* m_commandInterface;

    bool m_busy = false;
};

#endif // COMMUNICATIONENGINE_H
