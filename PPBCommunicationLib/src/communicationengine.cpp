#include "communicationengine.h"
#include <QMutex>
#include <QThread>

#include "../comm_dependencies.h"

static int techCommandType = qRegisterMetaType<TechCommand>("TechCommand");
// В начале communicationengine.cpp, после включений
class CommandHost : public CommandInterface {
public:
    CommandHost(communicationengine* engine, uint16_t address)
        : m_engine(engine), m_address(address) {}

    // --- Методы, которые делегируем движку с адресом ---
    void setParseResult(bool success, const QString& message) override {
        m_engine->setCommandParseResult(m_address, success, message);
    }

    void setParseData(const QVariant& parsedData) override {
        m_engine->setCommandParseData(m_address, parsedData);
    }

    void completeCurrentOperation(bool success, const QString& message) override {
        if (m_engine) {
            m_engine->completeOperation(m_address, success, message);
        }
    }

    // --- Методы, которые можно игнорировать или делегировать без адреса ---
    void setState(PPBState state) override {
        Q_UNUSED(state)
        // Состояние управляется движком, игнорируем
    }

    void startTimeoutTimer(int ms) override {
        Q_UNUSED(ms)
        // Таймеры в движке
    }

    void stopTimeoutTimer() override {
        // Игнорируем
    }

    void sendPacket(const QByteArray& packet, const QString& description) override {
        m_engine->sendPacketInternal(packet, description);
    }

    void sendDataPackets(const QVector<DataPacket>& packets) override {
        for (const DataPacket& pkt : packets) {
            QByteArray data(reinterpret_cast<const char*>(&pkt), sizeof(DataPacket));
            sendPacket(data, "Data packet");
        }
    }

    QVector<DataPacket> getGeneratedPackets() const override {
        return QVector<DataPacket>(); // заглушка
    }

    // --- Методы, связанные с уведомлениями о пакетах (не критичны, можно заглушить) ---
    void notifySentPackets(const QVector<DataPacket>& packets) override {
        Q_UNUSED(packets)
        LOG_TECH_DEBUG("notifySentPackets called, ignored");
    }

    void notifyReceivedPackets(const QVector<DataPacket>& packets) override {
        Q_UNUSED(packets)
        LOG_TECH_DEBUG("notifySentPackets called, ignored");
    }

    void requestClearPacketData() override {
        LOG_TECH_DEBUG("notifySentPackets called, ignored");
    }



private:
    communicationengine* m_engine;
    uint16_t m_address;
};


// Определения методов для Internal::StateManager
namespace Internal {

StateManager::StateManager(QObject* parent) : QObject(parent) {}

PPBState StateManager::getState(uint16_t address) const {
    QMutexLocker locker(&m_mutex);
    return m_states.value(address, PPBState::Idle);
}

void StateManager::setState(uint16_t address, PPBState state) {
    {
        QMutexLocker locker(&m_mutex);
        PPBState oldState = m_states.value(address, PPBState::Idle);
        if (oldState == state) return;
        m_states[address] = state;
    }
    emit stateChanged(address, state);
}

void StateManager::clear() {
    QMutexLocker locker(&m_mutex);
    m_states.clear();
}

CommandQueue::CommandQueue(QObject* parent) : QObject(parent) {}

CommandQueue::~CommandQueue() {
    clear(); // Очищаем все команды при уничтожении
}

void Internal::CommandQueue::enqueue(uint16_t address, std::unique_ptr<PPBCommand> cmd) {
    QMutexLocker locker(&m_mutex);
    m_queues[address].push_back(std::move(cmd));
    emit queueChanged(address, m_queues[address].size());
}

std::unique_ptr<PPBCommand> Internal::CommandQueue::dequeue(uint16_t address) {
    QMutexLocker locker(&m_mutex);

    auto it = m_queues.find(address);
    if (it != m_queues.end() && !it->second.empty()) {
        // Берем первый элемент из deque
        std::unique_ptr<PPBCommand> cmd = std::move(it->second.front());
        it->second.pop_front();

        if (it->second.empty()) {
            m_queues.erase(it);
        }

        emit queueChanged(address, m_queues.count(address) ? m_queues[address].size() : 0);
        return cmd;
    }

    return nullptr;
}
bool Internal::CommandQueue::isEmpty(uint16_t address) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_queues.find(address);
    return it == m_queues.end() || it->second.empty();
}

void Internal::CommandQueue::clear() {
    QMutexLocker locker(&m_mutex);
    m_queues.clear();
}

QList<uint16_t> Internal::CommandQueue::addresses() const {
    QMutexLocker locker(&m_mutex);
    QList<uint16_t> keys;
    for (const auto& pair : m_queues) {
        keys.append(pair.first);
    }
    return keys;
}

} // namespace Internal



communicationengine::communicationengine(UDPClient* udpClient, std::unique_ptr<IProtocolAdapter> adapter, QObject* parent)
    : QObject(parent)
    , m_udpClient(udpClient)
    , m_protocolAdapter(std::move(adapter))
    , m_queueTimer(nullptr)
    , m_commandInterface(nullptr)
    , m_currentAddress(0)
    , m_currentPort(0)
    , m_stateManager(new Internal::StateManager(this))
    , m_commandQueue(new Internal::CommandQueue(this))
{

    LOG_TECH_STATE("communicationengine created");

    if (m_udpClient) {
        connect(m_udpClient, &UDPClient::dataReceived,
                this, &communicationengine::onDataReceived, Qt::DirectConnection);
        connect(m_udpClient, &UDPClient::errorOccurred,
                this, &communicationengine::onNetworkError, Qt::DirectConnection);
    }

    m_queueTimer = new QTimer(this);
    m_queueTimer->setInterval(100);
    connect(m_queueTimer, &QTimer::timeout, this, &communicationengine::processCommandQueue);
    m_queueTimer->start();

}



communicationengine::~communicationengine() {
    m_queueTimer->stop();

    // Очищаем все контексты и их таймеры

    m_contexts.clear();
}


// Получаем контекст для адреса
communicationengine::PPBContext* communicationengine::getContext(uint16_t address) {
    //QMutexLocker locker(&m_contextsMutex);
    return &m_contexts[address];  // std::unordered_map автоматически создает элемент
}

// Очищаем контекст
void communicationengine::clearContext(uint16_t address) {
     LOG_TECH_DEBUG(QString("Clearing context for address 0x%1").arg(address, 4, 16, QChar('0')));
    //QMutexLocker locker(&m_contextsMutex);
    auto it = m_contexts.find(address);
    if (it != m_contexts.end()) {
        if (it->second.operationTimer) {
            it->second.operationTimer->stop();
           it->second.operationTimer.reset();
        }
        m_contexts.erase(it);
          LOG_TECH_DEBUG("Engine: Context deleeted");
    }
}

bool communicationengine::connectToPPB(uint16_t address, const QString& ip, quint16 port) {

    if (QThread::currentThread() != this->thread()) {
                bool result = false;
                QMetaObject::invokeMethod(this, "connectToPPB", Qt::BlockingQueuedConnection,
                                                                     Q_RETURN_ARG(bool, result),
                                                                     Q_ARG(uint16_t, address),
                                                                     Q_ARG(QString, ip),
                                                                     Q_ARG(quint16, port));
                return result;
            }


            LOG_UI_OPERATION(QString("Connecting to PPB %1:%2").arg(ip).arg(port));

            LOG_TECH_STATE(QString("connectToPPB: address=0x%1, IP=%2, port=%3").arg(address, 4, 16, QChar('0'))
                               .arg(ip).arg(port));

    m_currentAddress = address;
    m_currentIP = ip;
    m_currentPort = port;

    auto tsCommand = CommandFactory::create(TechCommand::TS);
    if (!tsCommand) {
        LOG_UI_ALERT("Failed to create TS command");
        return false;
    }

    m_commandQueue->enqueue(address, std::move(tsCommand));
    return true;
}

void communicationengine::disconnect() {

    if (QThread::currentThread() != this->thread()) {
                QMetaObject::invokeMethod(this, "disconnect", Qt::QueuedConnection);
                return;
            }

    LOG_UI_OPERATION("Disconnected from PPB");
    LOG_TECH_STATE("disconnect called");


    m_commandQueue->clear();
    m_stateManager->clear();
    emit disconnected();
}

void communicationengine::executeCommand(TechCommand cmd, uint16_t address) {

    if (QThread::currentThread() != this->thread()) {
                QMetaObject::invokeMethod(this, "executeCommand", Qt::QueuedConnection,
                                                                    Q_ARG(TechCommand, cmd),
                                                                   Q_ARG(uint16_t, address));
                return;
            }

    LOG_TECH_PROTOCOL(QString("executeCommand: cmd=%1, addr=0x%2").arg(int(cmd)).arg(address,4,16,QLatin1Char('0')));

    auto command = CommandFactory::create(cmd);
    if (!command) {
        emit errorOccurred(QString("Неизвестная команда: %1").arg(static_cast<int>(cmd)));
        return;
    }

    PPBState currentState = m_stateManager->getState(address);
    if (currentState != PPBState::Ready && currentState != PPBState::Idle) {
        m_commandQueue->enqueue(address, std::move(command));
       LOG_TECH_PROTOCOL(QString("Command %1 for address 0x%2 enqueued").arg(command->name()).arg(address,4,16,QLatin1Char('0')));
    } else {
        executeCommandImmediately(address, std::move(command));
    }
}

void communicationengine::sendFUTransmit(uint16_t address) {
    QByteArray packet = m_protocolAdapter->buildFURequest(address, 0, nullptr);
    sendPacketInternal(packet, "ФУ передача");
}
void communicationengine::sendFUReceiveImpl(uint16_t address, uint8_t period, const QByteArray& fuData)
{
    // Этот слот ВСЕГДА выполняется в потоке объекта, поэтому проверка не нужна
    const uint8_t* dataPtr = nullptr;
    uint8_t buffer[3];
    if (fuData.size() >= 3) {
        memcpy(buffer, fuData.constData(), 3);
        dataPtr = buffer;
    }
    // Вызываем основной метод уже в правильном потоке
    sendFUReceive(address, period, dataPtr);
}
void communicationengine::sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3]) {
    QByteArray packet = m_protocolAdapter->buildFURequest(address, period, fuData);
    sendPacketInternal(packet, "ФУ прием");
}

void communicationengine::setCommandParseResult(uint16_t address, bool success, const QString& message) {

    if (QThread::currentThread() != this->thread()) {
                QMetaObject::invokeMethod(this, "setCommandParseResult", Qt::QueuedConnection,
                                                                    Q_ARG(uint16_t, address),
                                                                    Q_ARG(bool, success),
                                                                    Q_ARG(QString, message));
                return;
            }

    PPBContext* context = getContext(address);
    if (!context) {
       LOG_TECH_DEBUG(QString("setCommandParseResult: no context for addr 0x%1").arg(address,4,16,QChar('0')));
        return;
    }

    LOG_TECH_DEBUG(QString("setCommandParseResult: addr=0x%1, success=%2, msg='%3'")
                  .arg(address, 4, 16, QChar('0'))
                  .arg(success)
                  .arg(message));

    context->parsedSuccess = success;
    context->parsedMessage = message;
}

void communicationengine::setCommandParseData(uint16_t address, const QVariant& data) {

    if (QThread::currentThread() != this->thread()) {
                QMetaObject::invokeMethod(this, "setCommandParseData", Qt::QueuedConnection,
                                                                     Q_ARG(uint16_t, address),
                                                                     Q_ARG(QVariant, data));
                return;
            }

    PPBContext* context = getContext(address);
    if (!context) {
       // LOG_CAT_WARNING("Engine",QString("setCommandParseData: нет контекста для адреса 0x%1")
                        //.arg(address, 4, 16, QChar('0')));
        return;
    }

    LOG_TECH_DEBUG(QString("setCommandParseData: addr=0x%1, data type=%2")
                  .arg(address, 4, 16, QChar('0'))
                  .arg(data.typeName()));

    context->parsedData = data;
}

// ===== ПРИВАТНЫЕ МЕТОДЫ =====

void communicationengine::executeCommandImmediately(uint16_t address, std::unique_ptr<PPBCommand> command) {
    if (!command) return;

    // Проверяем, можем ли выполнить команду
    PPBState currentState = m_stateManager->getState(address);
    if (currentState != PPBState::Ready && currentState != PPBState::Idle) {
        LOG_TECH_STATE(QString("Cannot execute %1 for 0x%2: state %3")
                        .arg(command->name())
                        .arg(address, 4, 16, QChar('0'))
                        .arg(stateToString(currentState)));
        return;
    }

    PPBContext* context = getContext(address);
    *context = PPBContext(); // очистка
    context->currentCommand = std::move(command);
    context->operationCompleted = false;
    // context->packetsExpected и т.д. больше не используются

    transitionState(address, PPBState::SendingCommand, "Начинаем " + context->currentCommand->name());
    updateBusyState();
    // Построение запроса через адаптер
    QByteArray request;
    if (/* это TU команда? */ true) { // нужно определять тип, но пока все TU через buildRequest
        request = context->currentCommand->buildRequest(address);
    } else {
        // для FU команд будет отдельный вызов
    }

    sendPacketInternal(request, context->currentCommand->name());

    // Таймаут
    context->operationTimer = std::make_unique<QTimer>();
    context->operationTimer->setSingleShot(true);
    connect(context->operationTimer.get(), &QTimer::timeout,
            this, [this, address]() { onOperationTimeout(address); });
    context->operationTimer->start(context->currentCommand->timeoutMs());
}

void communicationengine::processCommandQueue() {
    auto addresses = m_commandQueue->addresses();

    for (uint16_t address : addresses) {
        PPBState state = m_stateManager->getState(address);

        if ((state == PPBState::Ready || state == PPBState::Idle) &&
            !m_commandQueue->isEmpty(address)) {
            auto command = m_commandQueue->dequeue(address);
            if (command) {
                executeCommandImmediately(address, std::move(command));
            }
        }
    }
}

void communicationengine::onDataReceived(const QByteArray& data, const QHostAddress& sender, quint16 port) {
    Q_UNUSED(sender)
    Q_UNUSED(port)

   LOG_TECH_DEBUG(QString("onDataReceived: size=%1").arg(data.size()));

    // Получаем событие от адаптера
    ProtocolEvent event;
    if (!m_protocolAdapter->parseIncomingPacket(data, event)) {
            LOG_TECH_DEBUG("Packet not recognized, ignored");
        return;
    }

    // Определяем адрес, к которому относится событие
    uint16_t address = event.address;
    if (address == 0) {
        LOG_TECH_PROTOCOL("Event with zero address, ignored");
        return;
    }

    // Получаем контекст для этого адреса
    PPBContext* context = getContext(address);
    if (!context || !context->currentCommand || context->operationCompleted) {
        LOG_TECH_PROTOCOL(QString("Event for addr 0x%1 without active command").arg(address,4,16,QLatin1Char('0')));
        return;
    }

    // Обработка в зависимости от типа события
    switch (event.type) {
    case ProtocolEvent::Error:
        completeOperation(address, false,
                          QString("Ошибка ППБ: 0x%1").arg(event.status, 2, 16, QChar('0')));
        break;

    case ProtocolEvent::Ok:
        // Команда выполнена успешно без данных
        completeOperation(address, true, "Команда выполнена");
        break;

    case ProtocolEvent::Data:
        // Есть данные – передаём команде
        {
            {
                QVector<QByteArray> dataList;
                dataList.append(event.payload);
                CommandHost host(this, address);
                context->currentCommand->onDataReceived(&host, dataList);
                // После возврата из onDataReceived команда могла установить результаты через host
                completeOperation(address, context->parsedSuccess, context->parsedMessage);
            }
        break;

    case ProtocolEvent::BridgeResponse:
        LOG_TECH_PROTOCOL(QString("Bridge response: addr=0x%1, status=%2").arg(event.address,4,16,QLatin1Char('0')).arg(event.status));
        // Для бриджа не меняем состояние, просто эмитим сигнал
        emit commandCompleted(event.status == 1, "ФУ команда выполнена", TechCommand::TS); // TODO: передавать команду
        break;

    default:
        LOG_TECH_DEBUG("Unknown event type, ignored");;
        break;
    }
}
}
void communicationengine::onNetworkError(const QString& error) {
    emit errorOccurred(error);
}

void communicationengine::onOperationTimeout(uint16_t address) {
    PPBContext* context = getContext(address);
    if (!context || !context->currentCommand) {
        transitionState(address, PPBState::Ready, "Таймаут без активной команды");
        return;
    }

    LOG_TECH_STATE(QString("Timeout for command %1 (0x%2)").arg(context->currentCommand->name()).arg(address,4,16,QLatin1Char('0')));
    completeOperation(address, false, "Таймаут операции");
}

void communicationengine::sendPacketInternal(const QByteArray& packet, const QString& description) {

    if (QThread::currentThread() != this->thread()) {
                QMetaObject::invokeMethod(this, "sendPacketInternal", Qt::QueuedConnection,
                                                                     Q_ARG(QByteArray, packet),
                                                                     Q_ARG(QString, description));
                return;
            }

    if (!m_udpClient) {
        emit errorOccurred("UDPClient не инициализирован");
        return;
    }

    if (m_currentIP.isEmpty() || m_currentIP == "255.255.255.255") {
        m_udpClient->sendBroadcast(packet, m_currentPort);
    } else {
        m_udpClient->sendTo(packet, m_currentIP, m_currentPort);
    }

    LOG_TECH_DEBUG(QString("Packet sent: %1 (%2 bytes)").arg(description).arg(packet.size()));
}

// +++++++++++++++++++++++++++++++++++++++++++++++++ МАШИНА СОСТОЯНИЯ +++++++++++++++++++++++++++++++++++++
QString communicationengine::stateToString(PPBState state)  const {
    switch (state) {
    case PPBState::Idle: return "Idle";
    case PPBState::Ready: return "Ready";
    case PPBState::SendingCommand: return "SendingCommand";
    default: return "Unknown";
    }
}

void communicationengine::transitionState(uint16_t address, PPBState newState, const QString& reason) {
    // Получаем текущее состояние
    PPBState oldState = m_stateManager->getState(address);

    // Если состояние не изменилось - выходим
    if (oldState == newState) {
        LOG_TECH_DEBUG(QString("State unchanged for 0x%1: %2 [%3]")
                      .arg(address, 4, 16, QChar('0'))
                      .arg(stateToString(oldState))
                      .arg(reason));
        return;
    }

    // Логируем переход
    LOG_TECH_STATE(QString("State transition for 0x%1: %2 -> %3 [%4]")
                 .arg(address, 4, 16, QChar('0'))
                 .arg(stateToString(oldState))
                 .arg(stateToString(newState))
                 .arg(reason));

    // Дополнительные действия при определенных переходах
    switch (newState) {
    case PPBState::Idle:
        // При переходе в Idle очищаем контекст
        clearContext(address);
        break;

    case PPBState::Ready:
        // При готовности проверяем очередь команд
        QTimer::singleShot(0, this, [this, address]() {
            processNextCommandForAddress(address);
        });
        break;

    default:
        break;
    }

    // Устанавливаем новое состояние
    m_stateManager->setState(address, newState);

    // Отправляем сигнал (если нужно)
    if (address == m_currentAddress) {
        emit stateChanged(address, newState);
    }
}

void communicationengine::completeOperation(uint16_t address, bool success, const QString& message) {
    PPBContext* context = getContext(address);
    if (!context || context->operationCompleted) return;

    context->operationCompleted = true;
    if (context->operationTimer) context->operationTimer->stop();

    QString finalMessage = message;
    if (context->parsedSuccess) {
        finalMessage = context->parsedMessage.isEmpty() ? finalMessage : context->parsedMessage;
    } else if (!context->parsedMessage.isEmpty()) {
        finalMessage = context->parsedMessage;
    }
    LOG_TECH_STATE(QString("Operation completed for 0x%1: %2 - %3").arg(address,4,16,QChar('0')).arg(success?"success":"fail").arg(finalMessage));

        if(success) LOG_UI_RESULT(message);
            else LOG_UI_ALERT(message);

        emit commandCompleted(success, finalMessage, context->currentCommand->commandId());

    if (context->parsedData.isValid()) {
        emit commandDataParsed(address, context->parsedData, context->currentCommand->commandId());
    }

    if (context->currentCommand->commandId() == TechCommand::TS && success) {
        QVariantMap map = context->parsedData.toMap();
        uint16_t mask = map.value("mask").toUInt();
        QVariantList list = map.value("packets").toList();
        QVector<QByteArray> data;
        for (const auto& item : list) {
            data.append(item.toByteArray());
        }
        emit statusReceived(address, mask, data);
    }

    PPBState nextState = success ? PPBState::Ready : PPBState::Idle;
    transitionState(address, nextState, "Завершение операции");

    context->clearParseResults();
    updateBusyState();
}
void communicationengine::processNextCommandForAddress(uint16_t address) {
    // Проверяем, что мы в состоянии Ready
    if (m_stateManager->getState(address) != PPBState::Ready) {
        return;
    }

    // Проверяем, есть ли команды в очереди
    if (m_commandQueue->isEmpty(address)) {
        return;
    }

    // Берем следующую команду из очереди
    auto command = m_commandQueue->dequeue(address);
    if (!command) {
        return;
    }

   LOG_TECH_PROTOCOL(QString("Dequeue command for 0x%1: %2")
                 .arg(address, 4, 16, QChar('0'))
                 .arg(command->name()));

    // Выполняем команду немедленно
    executeCommandImmediately(address, std::move(command));
}

bool communicationengine::canExecuteCommand(uint16_t address, const PPBCommand* command) const {
    Q_UNUSED(address)
    Q_UNUSED(command)
    // В новой архитектуре команды можно выполнять всегда, если состояние позволяет
    return true;
}

void communicationengine::notifySentPackets(const QVector<DataPacket>& packets) {
    emit sentPacketsSaved(packets);
    LOG_TECH_DEBUG(QString("Notified %1 sent packets").arg(packets.size()));
}

void communicationengine::notifyReceivedPackets(const QVector<DataPacket>& packets) {
    emit receivedPacketsSaved(packets);
    LOG_TECH_DEBUG(QString("Notified %1 received packets").arg(packets.size()));
}

void communicationengine::requestClearPacketData() {
    emit clearPacketDataRequested();
    LOG_TECH_DEBUG("Clear packet data requested");
}
void communicationengine::sendDataPackets(const QVector<DataPacket>& packets) {
    for (const DataPacket& pkt : packets) {
        QByteArray data(reinterpret_cast<const char*>(&pkt), sizeof(DataPacket));
        sendPacketInternal(data, "Data packet");
    }
    emit sentPacketsSaved(packets);
}
void communicationengine::updateBusyState() {
    Q_ASSERT(thread() == QThread::currentThread());
    bool busy = false;
    for (const auto& pair : m_contexts) {
        if (pair.second.currentCommand && !pair.second.operationCompleted) {
            busy = true;
            break;
        }
    }
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(busy);
    }
}

PPBState communicationengine::overallState() const
{
    QMutexLocker locker(&m_mutex);
    bool hasReady = false;
    bool hasSending = false;
    for (const auto& state : m_stateManager->states()) {
        if (state == PPBState::Ready) hasReady = true;
        else if (state == PPBState::SendingCommand) hasSending = true;
    }
    if (hasReady) return PPBState::Ready;
    if (hasSending) return PPBState::SendingCommand;
    return PPBState::Idle;
}
