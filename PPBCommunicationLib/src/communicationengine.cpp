#include "communicationengine.h"
#include <QMutex>
#include <QThread>
#include <QtEndian>
#include "../comm_dependencies.h"

static int techCommandType = qRegisterMetaType<TechCommand>("TechCommand");

class CommandHost : public CommandInterface {
public:
    CommandHost(communicationengine* engine, uint16_t address)
        : m_engine(engine), m_address(address) {}

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

    void setState(PPBState state) override {
        Q_UNUSED(state)
    }

    void startTimeoutTimer(int ms) override {
        Q_UNUSED(ms)
    }

    void stopTimeoutTimer() override {}

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
        return QVector<DataPacket>();
    }

    void notifySentPackets(const QVector<DataPacket>& packets) override {
        Q_UNUSED(packets)
        LOG_TECH_DEBUG("notifySentPackets called, ignored");
    }

    void notifyReceivedPackets(const QVector<DataPacket>& packets) override {
        Q_UNUSED(packets)
        LOG_TECH_DEBUG("notifyReceivedPackets called, ignored");
    }

    void requestClearPacketData() override {
        LOG_TECH_DEBUG("requestClearPacketData called, ignored");
    }

private:
    communicationengine* m_engine;
    uint16_t m_address;
};

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
    clear();
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

void Internal::CommandQueue::clear(uint16_t address)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_queues.find(address);
    if (it != m_queues.end()) {
        it->second.clear();
        m_queues.erase(it);
    }
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
                this, &communicationengine::onDataReceived, Qt::AutoConnection);
        connect(m_udpClient, &UDPClient::errorOccurred,
                this, &communicationengine::onNetworkError, Qt::AutoConnection);
    }

    m_queueTimer = new QTimer(this);
    m_queueTimer->setInterval(100);
    connect(m_queueTimer, &QTimer::timeout, this, &communicationengine::processCommandQueue);
    m_queueTimer->start();
}

communicationengine::~communicationengine() {
    m_queueTimer->stop();
    m_contexts.clear();
}

communicationengine::PPBContext* communicationengine::findContext(uint16_t address) {
    QMutexLocker locker(&m_contextsMutex);
    auto it = m_contexts.find(address);
    return (it != m_contexts.end()) ? &it->second : nullptr;
}

communicationengine::PPBContext& communicationengine::getOrCreateContext(uint16_t address) {
    QMutexLocker locker(&m_contextsMutex);
    return m_contexts[address];
}

void communicationengine::clearContext(uint16_t address) {
    LOG_TECH_DEBUG(QString("Clearing context for address 0x%1").arg(address, 4, 16, QChar('0')));
    QMutexLocker locker(&m_contextsMutex);
    auto it = m_contexts.find(address);
    if (it != m_contexts.end()) {
        if (it->second.operationTimer) {
            it->second.operationTimer->stop();
            it->second.operationTimer.reset();
        }
        m_contexts.erase(it);
        LOG_TECH_DEBUG("Engine: Context deleted");
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
    m_pendingGroupCommands.clear();
    m_groupOperations.clear();
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
    QString cmdName = command->name();

    // Address 0 is used by the firmware VOLUME/UPDATE/CLEAN stream. Do not let
    // it overtake an active or queued group command (VERS/PROGRAMM). Otherwise
    // the first VOLUME may hit the bridge while selected PPBs are still entering
    // the bootloader.
    if (address == 0 && (!m_groupOperations.empty() || !m_pendingGroupCommands.empty())) {
        m_commandQueue->enqueue(address, std::move(command));
        LOG_TECH_PROTOCOL(QString("Command %1 for address 0x0000 gated behind group operations").arg(cmdName));
        return;
    }

    PPBState currentState = m_stateManager->getState(address);
    if (currentState != PPBState::Ready && currentState != PPBState::Idle) {
        m_commandQueue->enqueue(address, std::move(command));
        LOG_TECH_PROTOCOL(QString("Command %1 for address 0x%2 enqueued").arg(cmdName).arg(address,4,16,QLatin1Char('0')));
    } else {
        executeCommandImmediately(address, std::move(command));
    }
}

void communicationengine::executeCommand(TechCommand cmd, uint16_t address, const QByteArray& data) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this,
                                  [this, cmd, address, data]() {
                                      executeCommand(cmd, address, data);
                                  },
                                  Qt::QueuedConnection);
        return;
    }

    auto command = std::make_unique<DataCommand>(cmd, data);

    if (address == 0 && (!m_groupOperations.empty() || !m_pendingGroupCommands.empty())) {
        m_commandQueue->enqueue(address, std::move(command));
        return;
    }

    PPBState currentState = m_stateManager->getState(address);
    if (currentState != PPBState::Ready && currentState != PPBState::Idle) {
        m_commandQueue->enqueue(address, std::move(command));
    } else {
        executeCommandImmediately(address, std::move(command));
    }
}

void communicationengine::sendFUTransmit(uint16_t address, uint8_t period, const uint8_t fuData[3]) {
    QByteArray packet = m_protocolAdapter->buildFURequest(address,1, period, fuData);
    sendPacketInternal(packet, "ФУ передача");
}

void communicationengine::sendFUReceiveImpl(uint16_t address, uint8_t period, const QByteArray& fuData)
{
    const uint8_t* dataPtr = nullptr;
    uint8_t buffer[3];
    if (fuData.size() >= 3) {
        memcpy(buffer, fuData.constData(), 3);
        dataPtr = buffer;
    }
    sendFUReceive(address, period, dataPtr);
}

void communicationengine::sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3]) {
    QByteArray packet = m_protocolAdapter->buildFURequest(address,0, period, fuData);
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

    PPBContext* context = findContext(address);
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

    PPBContext* context = findContext(address);
    if (!context) {
        return;
    }

    LOG_TECH_DEBUG(QString("setCommandParseData: addr=0x%1, data type=%2")
                  .arg(address, 4, 16, QChar('0'))
                  .arg(data.typeName()));

    context->parsedData = data;
}

quint64 communicationengine::executeGroupCommand(TechCommand cmd, uint16_t mask, const QByteArray& data)
{
    if (QThread::currentThread() != this->thread()) {
        quint64 result = 0;
        QMetaObject::invokeMethod(this, "executeGroupCommand", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(quint64, result),
                                  Q_ARG(TechCommand, cmd),
                                  Q_ARG(uint16_t, mask),
                                  Q_ARG(QByteArray, data));
        return result;
    }

    if (mask == 0) {
        emit errorOccurred("Group command: empty mask");
        return 0;
    }

    if (!CommandFactory::create(cmd)) {
        emit errorOccurred(QString("Group command: unknown command %1").arg(static_cast<int>(cmd)));
        return 0;
    }

    const quint64 groupId = m_nextGroupId++;

    if (!canStartGroupCommand(mask)) {
        PendingGroupCommand pending;
        pending.groupId = groupId;
        pending.command = cmd;
        pending.mask = mask;
        pending.data = data;
        m_pendingGroupCommands.push_back(std::move(pending));
        LOG_TECH_PROTOCOL(QString("Group command %1 queued, id=%2, mask=0x%3")
                              .arg(static_cast<int>(cmd))
                              .arg(groupId)
                              .arg(mask, 4, 16, QChar('0')));
        return groupId;
    }

    if (!startGroupCommand(groupId, cmd, mask, data)) {
        emit groupCommandCompleted(groupId, false, "Не удалось запустить групповую команду");
    }

    return groupId;
}

bool communicationengine::canStartGroupCommand(uint16_t mask) const
{
    for (int i = 0; i < 16; ++i) {
        if (!(mask & (1u << i))) {
            continue;
        }
        const uint16_t address = static_cast<uint16_t>(1u << i);
        const PPBState state = m_stateManager->getState(address);
        if (state != PPBState::Ready && state != PPBState::Idle) {
            return false;
        }
    }
    return mask != 0;
}

bool communicationengine::startGroupCommand(quint64 groupId, TechCommand cmd, uint16_t mask, const QByteArray& data)
{
    if (!canStartGroupCommand(mask)) {
        return false;
    }

    QList<uint16_t> addresses;
    for (int i = 0; i < 16; ++i) {
        if (mask & (1u << i)) {
            addresses.append(static_cast<uint16_t>(1u << i));
        }
    }

    if (addresses.isEmpty() || !CommandFactory::create(cmd)) {
        return false;
    }

    GroupInfo group;
    group.pendingAddresses = QSet<uint16_t>(addresses.begin(), addresses.end());
    m_groupOperations[groupId] = std::move(group);

    for (uint16_t address : addresses) {
        auto command = CommandFactory::create(cmd);
        if (!command) {
            m_groupOperations.erase(groupId);
            return false;
        }
        executeCommandImmediately(address, std::move(command), true);
    }

    QByteArray request;
    if (data.isEmpty()) {
        request = PacketBuilder::createTURequest(mask, cmd);
    } else {
        request = PacketBuilder::createTURequestWithData(mask, cmd, data);
    }

    sendPacketInternal(request, QString("Group command %1").arg(static_cast<int>(cmd)));
    LOG_TECH_PROTOCOL(QString("Group command %1 started, id=%2, mask=0x%3")
                          .arg(static_cast<int>(cmd))
                          .arg(groupId)
                          .arg(mask, 4, 16, QChar('0')));
    return true;
}

void communicationengine::tryStartPendingGroupCommands()
{
    if (m_pendingGroupCommands.empty()) {
        return;
    }

    const PendingGroupCommand& front = m_pendingGroupCommands.front();
    if (!canStartGroupCommand(front.mask)) {
        return;
    }

    PendingGroupCommand pending = std::move(m_pendingGroupCommands.front());
    m_pendingGroupCommands.pop_front();

    if (!startGroupCommand(pending.groupId, pending.command, pending.mask, pending.data)) {
        emit groupCommandCompleted(pending.groupId, false, "Не удалось запустить отложенную групповую команду");
    }
}

void communicationengine::executeCommandImmediately(uint16_t address, std::unique_ptr<PPBCommand> command, bool suppressSend) {
    if (!command) return;

    PPBState currentState = m_stateManager->getState(address);
    if (currentState != PPBState::Ready && currentState != PPBState::Idle) {
        LOG_TECH_STATE(QString("Cannot execute %1 for 0x%2: state %3")
                        .arg(command->name())
                        .arg(address, 4, 16, QChar('0'))
                        .arg(stateToString(currentState)));
        return;
    }

    PPBContext& context = getOrCreateContext(address);
    context = PPBContext();
    context.currentCommand = std::move(command);
    context.suppressSend = suppressSend;
    context.operationCompleted = false;

    transitionState(address, PPBState::SendingCommand, "Начинаем " + context.currentCommand->name());
    updateBusyState();

    QByteArray request = context.currentCommand->buildRequest(address);
    if (request.isEmpty()) {
        completeOperation(address, false, "Не удалось сформировать пакет команды");
        return;
    }

    if (!context.suppressSend) {
        // VOLUME is a fire-and-stream packet. Its completion is local transport
        // completion, not an MCU ACK. Check writeDatagram() directly so a dead
        // or unbound socket cannot be reported as a successful firmware block.
        if (context.currentCommand->commandId() == TechCommand::VOLUME) {
            if (!m_udpClient) {
                VolumeCommand::clearCurrentVolumeData();
                completeOperation(address, false, "VOLUME: UDPClient не инициализирован");
                return;
            }

            qint64 bytesSent = -1;
            if (m_currentIP.isEmpty() || m_currentIP == "255.255.255.255") {
                bytesSent = m_udpClient->sendBroadcast(request, m_currentPort);
            } else {
                bytesSent = m_udpClient->sendTo(request, m_currentIP, m_currentPort);
            }

            VolumeCommand::clearCurrentVolumeData();
            if (bytesSent != request.size()) {
                completeOperation(address, false,
                                  QString("VOLUME: ошибка локальной UDP-отправки (%1/%2 байт)")
                                      .arg(bytesSent).arg(request.size()));
                return;
            }

            completeOperation(address, true, "VOLUME передан в UDP; ACK блока не ожидается");
            return;
        }

        sendPacketInternal(request, context.currentCommand->name());
    }

    context.operationTimer = std::make_unique<QTimer>();
    context.operationTimer->setSingleShot(true);
    connect(context.operationTimer.get(), &QTimer::timeout,
            this, [this, address]() { onOperationTimeout(address); });
    context.operationTimer->start(context.currentCommand->timeoutMs());
}

void communicationengine::processCommandQueue() {
    auto addresses = m_commandQueue->addresses();

    for (uint16_t address : addresses) {
        // Keep address 0 behind every active/pending group command. This is the
        // firmware ordering barrier between PROGRAMM and the VOLUME stream.
        if (address == 0 && (!m_groupOperations.empty() || !m_pendingGroupCommands.empty())) {
            continue;
        }

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

void communicationengine::onDataReceived(const QByteArray& data, const QHostAddress& sender, quint16 port)
{
    Q_UNUSED(sender)
    Q_UNUSED(port)

    if (data.size() < 2) {
        LOG_TECH_DEBUG("Packet too short, ignored");
        return;
    }

    const uint16_t addressFromPacket = qFromBigEndian<quint16>(
        reinterpret_cast<const uchar*>(data.constData()));
    PPBContext* context = findContext(addressFromPacket);

    ProtocolEvent event;
    bool handled = false;

    if (context && context->currentCommand && !context->operationCompleted) {
        TechCommand cmd = context->currentCommand->commandId();

        if (cmd == TechCommand::IS_YOU) {
            if (m_protocolAdapter->parseBridgeResponse(data, event)) {
                bool success = true;
                QString message;
                QVariant parsedData;
                if (event.payload.size() == 2) {
                    uint16_t mask = (static_cast<uint8_t>(event.payload[0]) << 8) | static_cast<uint8_t>(event.payload[1]);
                    QVariantMap map;
                    map["mask"] = mask;
                    parsedData = map;
                    message = QString("Маска активных ППБ: 0x%1").arg(mask, 4, 16, QChar('0'));
                } else if (event.payload == "ERR") {
                    message = "Необъяснимая ошибка от бриджа (ERR)";
                    success = false;
                } else {
                    message = "Неизвестный ответ бриджа";
                    success = false;
                }
                setCommandParseResult(addressFromPacket, success, message);
                setCommandParseData(addressFromPacket, parsedData);
                completeOperation(addressFromPacket, success, message);
                handled = true;
            }
            if (handled) return;
        }

        if (m_protocolAdapter->parseTUResponse(data, event)) {
            // Current 4-byte TU ACK format echoes the command in byte 3.
            // A delayed UDP ACK from a previous operation must not complete
            // the command that happens to be active now on the same address.
            if (data.size() == 4 && event.type == ProtocolEvent::Ok &&
                event.command != static_cast<quint8>(cmd)) {
                LOG_TECH_DEBUG(QString("Stale/mismatched TU ACK ignored: addr=0x%1 expected=0x%2 got=0x%3")
                                   .arg(addressFromPacket, 4, 16, QChar('0'))
                                   .arg(static_cast<quint8>(cmd), 2, 16, QChar('0'))
                                   .arg(event.command, 2, 16, QChar('0')));
                return;
            }

            switch (event.type) {
            case ProtocolEvent::Error:
                completeOperation(addressFromPacket, false,
                                  QString("Ошибка ППБ: 0x%1 (%2)")
                                      .arg(event.status, 2, 16, QChar('0'))
                                      .arg(errorCodeToString(event.status)));
                break;
            case ProtocolEvent::Ok:
                if (context->currentCommand) {
                    CommandHost host(this, addressFromPacket);
                    context->currentCommand->onOkReceived(&host, addressFromPacket);
                    if (!context->operationCompleted) {
                        completeOperation(addressFromPacket, true, "Команда выполнена");
                    }
                } else {
                    completeOperation(addressFromPacket, true, "Команда выполнена");
                }
                break;
            case ProtocolEvent::Data: {
                QVector<QByteArray> dataList;
                dataList.append(event.payload);
                CommandHost host(this, addressFromPacket);
                context->currentCommand->onDataReceived(&host, dataList);
                if (!context->operationCompleted) {
                    completeOperation(addressFromPacket, context->parsedSuccess, context->parsedMessage);
                }
                break;
            }
            default:
                LOG_TECH_DEBUG("Unknown TU event type, ignored");
            }
            handled = true;
        } else {
            if (m_protocolAdapter->parseBridgeResponse(data, event)) {
                completeOperation(addressFromPacket, false, "Неожиданный Bridge-ответ");
                handled = true;
            }
        }
        if (handled) return;
    }

    if (m_protocolAdapter->parseBridgeResponse(data, event)) {
        uint16_t bridgeAddress = event.address;
        PPBContext* ctx = findContext(bridgeAddress);
        if (ctx && ctx->currentCommand && !ctx->operationCompleted) {
            bool success = (event.status == 1);
            if (ctx->currentCommand->commandId() == TechCommand::IS_YOU) {
                success = true;
            }
            QString message;
            QVariant parsedData;
            if (event.payload.size() == 2) {
                uint16_t mask = (static_cast<uint8_t>(event.payload[0]) << 8) | static_cast<uint8_t>(event.payload[1]);
                QVariantMap map;
                map["mask"] = mask;
                parsedData = map;
                message = QString("Маска активных ППБ: 0x%1").arg(mask, 4, 16, QChar('0'));
            } else if (event.payload == "ERR") {
                message = "Необъяснимая ошибка от бриджа (ERR)";
                success = false;
            } else {
                message = success ? "Команда выполнена" : "Ошибка команды";
            }
            setCommandParseResult(bridgeAddress, success, message);
            setCommandParseData(bridgeAddress, parsedData);
            completeOperation(bridgeAddress, success, message);
        } else {
            emit fuCommandCompleted(event.address, event.command, event.status == 1,
                                    event.status == 1 ? "ФУ команда выполнена" : "Ошибка ФУ");
        }
        handled = true;
    }

    if (!handled) {
        LOG_TECH_DEBUG("Packet not recognized, ignored");
    }
}

QString communicationengine::errorCodeToString(uint8_t code) const {
    switch (code) {
    case 0x00: return "OK";
    case 0x01: return "Нет ответа от ППБ";
    case 0x02: return "Неверный формат сообщения";
    case 0x03: return "ППБ не ожидал такой команды";
    case 0x04: return "Неизвестная команда";
    default:   return QString("Неизвестный код ошибки 0x%1").arg(code, 2, 16, QChar('0'));
    }
}

void communicationengine::onNetworkError(const QString& error) {
    emit errorOccurred(error);
}

void communicationengine::onOperationTimeout(uint16_t address)
{
    PPBContext* context = findContext(address);
    if (!context || !context->currentCommand || context->operationCompleted) {
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
}

QString communicationengine::stateToString(PPBState state) const {
    switch (state) {
    case PPBState::Idle: return "Idle";
    case PPBState::Ready: return "Ready";
    case PPBState::SendingCommand: return "SendingCommand";
    default: return "Unknown";
    }
}

void communicationengine::transitionState(uint16_t address, PPBState newState, const QString& reason) {
    PPBState oldState = m_stateManager->getState(address);

    if (oldState == newState) {
        LOG_TECH_DEBUG(QString("State unchanged for 0x%1: %2 [%3]")
                      .arg(address, 4, 16, QChar('0'))
                      .arg(stateToString(oldState))
                      .arg(reason));
        return;
    }

    LOG_TECH_STATE(QString("State transition for 0x%1: %2 -> %3 [%4]")
                 .arg(address, 4, 16, QChar('0'))
                 .arg(stateToString(oldState))
                 .arg(stateToString(newState))
                 .arg(reason));

    switch (newState) {
    case PPBState::Idle:
        clearContext(address);
        break;

    case PPBState::Ready:
        QTimer::singleShot(0, this, [this, address]() {
            processNextCommandForAddress(address);
        });
        break;

    default:
        break;
    }

    m_stateManager->setState(address, newState);

    if (address == m_currentAddress) {
        emit stateChanged(address, newState);
    }
}

void communicationengine::completeOperation(uint16_t address, bool success, const QString& message) {
    PPBContext* context = findContext(address);
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

    emit commandCompleted(success, finalMessage, context->currentCommand->commandId());

    if (context->parsedData.isValid()) {
        emit commandDataParsed(address, context->parsedData, context->currentCommand->commandId());
    }

    if (context->currentCommand->commandId() == TechCommand::TS && success) {
        QVariantMap map = context->parsedData.toMap();
        uint32_t mask = map.value("mask").toUInt();
        QVariantList list = map.value("packets").toList();
        QVector<QByteArray> data;
        for (const auto& item : list) {
            data.append(item.toByteArray());
        }
        emit statusReceived(address, mask, data);
    }

    context->clearParseResults();
    updateBusyState();

    PPBState nextState = success ? PPBState::Ready : PPBState::Idle;
    transitionState(address, nextState, "Завершение операции");

    for (auto it = m_groupOperations.begin(); it != m_groupOperations.end(); ) {
        if (it->second.pendingAddresses.contains(address)) {
            it->second.pendingAddresses.remove(address);
            it->second.results[address] = success;
            it->second.messages[address] = finalMessage;

            if (it->second.pendingAddresses.isEmpty()) {
                bool allSuccess = true;
                QString summary;
                for (auto res : it->second.results) {
                    if (!res) { allSuccess = false; break; }
                }
                emit groupCommandCompleted(it->first, allSuccess, summary);
                it = m_groupOperations.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    tryStartPendingGroupCommands();

    // If no group remains, release any firmware/control command waiting at
    // address 0 without waiting for the 100 ms polling timer.
    if (m_groupOperations.empty() && m_pendingGroupCommands.empty()) {
        QTimer::singleShot(0, this, [this]() { processCommandQueue(); });
    }
}

void communicationengine::processNextCommandForAddress(uint16_t address) {
    if (m_stateManager->getState(address) != PPBState::Ready) {
        return;
    }

    if (m_commandQueue->isEmpty(address)) {
        return;
    }

    auto command = m_commandQueue->dequeue(address);
    if (!command) {
        return;
    }

    LOG_TECH_PROTOCOL(QString("Dequeue command for 0x%1: %2")
                 .arg(address, 4, 16, QChar('0'))
                 .arg(command->name()));

    executeCommandImmediately(address, std::move(command));
}

bool communicationengine::canExecuteCommand(uint16_t address, const PPBCommand* command) const {
    Q_UNUSED(address)
    Q_UNUSED(command)
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
    QMutexLocker locker(&m_contextsMutex);
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

void communicationengine::setBridgeAddress(const QString &ip, quint16 port)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "setBridgeAddress", Qt::QueuedConnection,
                                  Q_ARG(QString, ip), Q_ARG(quint16, port));
        return;
    }
    m_currentIP = ip;
    m_currentPort = port;
    LOG_TECH_DEBUG(QString("communicationengine: bridge address set to %1:%2").arg(ip).arg(port));
}

void communicationengine::clearCommandQueue(uint16_t address)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "clearCommandQueue", Qt::QueuedConnection,
                                  Q_ARG(uint16_t, address));
        return;
    }
    m_commandQueue->clear(address);
}
