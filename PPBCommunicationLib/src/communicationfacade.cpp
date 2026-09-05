#include "communicationfacade.h"
#include <QThread>
#include <QMetaObject>

// Вспомогательный шаблон для безопасного вызова в потоке фасада
template<typename Func>
void CommunicationFacade::invokeInThread(Func&& func) {
    if (QThread::currentThread() == thread()) {
        func();
    } else {
        QMetaObject::invokeMethod(this, std::forward<Func>(func), Qt::QueuedConnection);
    }
}

CommunicationFacade::CommunicationFacade(UDPClient* udpClient,
                                         std::unique_ptr<IProtocolAdapter> adapter,
                                         QObject* parent)
    : ICommunication(parent)
    , m_ownsUdpClient(false)
    , m_udpClient(udpClient)
    , m_engine(new communicationengine(m_udpClient, std::move(adapter), this))
{
    connect(m_engine, &communicationengine::stateChanged,
            this, &CommunicationFacade::onEngineStateChanged);
    connect(m_engine, &communicationengine::commandCompleted,
            this, &CommunicationFacade::onEngineCommandCompleted);
    connect(m_engine, &communicationengine::errorOccurred,
            this, &CommunicationFacade::onEngineErrorOccurred);
    connect(m_engine, &communicationengine::commandDataParsed,
            this, &CommunicationFacade::onEngineCommandDataParsed);
    connect(m_engine, &communicationengine::sentPacketsSaved,
            this, &CommunicationFacade::onEngineSentPacketsSaved);
    connect(m_engine, &communicationengine::receivedPacketsSaved,
            this, &CommunicationFacade::onEngineReceivedPacketsSaved);
    connect(m_engine, &communicationengine::clearPacketDataRequested,
            this, &CommunicationFacade::onEngineClearPacketDataRequested);
    connect(m_engine, &communicationengine::statusReceived,
            this, &CommunicationFacade::statusReceived);
    connect(m_engine, &communicationengine::commandProgress,
            this, &CommunicationFacade::onEngineCommandProgress);
    connect(m_engine, &communicationengine::busyChanged,
            this, &CommunicationFacade::onEngineBusyChanged);
    connect(m_engine, &communicationengine::groupCommandCompleted,
            this, &CommunicationFacade::onEngineGroupCommandCompleted);
}

CommunicationFacade::CommunicationFacade(QObject* parent)
    : ICommunication(parent)
    , m_ownsUdpClient(true)
    , m_udpClient(new UDPClient(this))
    , m_engine(nullptr)
{
}

CommunicationFacade::~CommunicationFacade()
{
}

void CommunicationFacade::initialize()
{
    if (m_engine) {
        qWarning() << "CommunicationFacade already initialized";
        return;
    }

    auto adapter = std::make_unique<ProtocolAdapter>();
    m_engine = new communicationengine(m_udpClient, std::move(adapter), this);

    connect(m_engine, &communicationengine::stateChanged,
            this, &CommunicationFacade::onEngineStateChanged);
    connect(m_engine, &communicationengine::commandCompleted,
            this, &CommunicationFacade::onEngineCommandCompleted);
    connect(m_engine, &communicationengine::errorOccurred,
            this, &CommunicationFacade::onEngineErrorOccurred);
    connect(m_engine, &communicationengine::commandDataParsed,
            this, &CommunicationFacade::onEngineCommandDataParsed);
    connect(m_engine, &communicationengine::sentPacketsSaved,
            this, &CommunicationFacade::onEngineSentPacketsSaved);
    connect(m_engine, &communicationengine::receivedPacketsSaved,
            this, &CommunicationFacade::onEngineReceivedPacketsSaved);
    connect(m_engine, &communicationengine::clearPacketDataRequested,
            this, &CommunicationFacade::onEngineClearPacketDataRequested);
    connect(m_engine, &communicationengine::statusReceived,
            this, &CommunicationFacade::statusReceived);
    connect(m_engine, &communicationengine::commandProgress,
            this, &CommunicationFacade::onEngineCommandProgress);
    connect(m_engine, &communicationengine::busyChanged,
            this, &CommunicationFacade::onEngineBusyChanged);
    connect(m_engine, &communicationengine::groupCommandCompleted,
            this, &CommunicationFacade::onEngineGroupCommandCompleted);

    QMetaObject::invokeMethod(m_udpClient, "initializeInThread", Qt::QueuedConnection);
    connect(m_udpClient, &UDPClient::initialized, this, &CommunicationFacade::initialized);
}

bool CommunicationFacade::connectToPPB(uint16_t address, const QString& ip, quint16 port)
{
    if (QThread::currentThread() != thread()) {
        bool result = false;
        QMetaObject::invokeMethod(this, [this, address, ip, port, &result]() {
            result = m_engine->connectToPPB(address, ip, port);
        }, Qt::BlockingQueuedConnection);
        return result;
    }
    return m_engine->connectToPPB(address, ip, port);
}

void CommunicationFacade::disconnect()
{
    invokeInThread([this]() { m_engine->disconnect(); });
}

void CommunicationFacade::executeCommand(TechCommand cmd, uint16_t address)
{
    invokeInThread([this, cmd, address]() { m_engine->executeCommand(cmd, address); });
}

void CommunicationFacade::executeCommand(TechCommand cmd, uint16_t address, const QByteArray& data) {
    invokeInThread([this, cmd, address, data]() {
        m_engine->executeCommand(cmd, address, data);
    });
}

quint64 CommunicationFacade::executeGroupCommand(TechCommand cmd, uint16_t mask, const QByteArray& data)
{
    if (QThread::currentThread() != thread()) {
        quint64 result = 0;
        QMetaObject::invokeMethod(this, "executeGroupCommand", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(quint64, result),
                                  Q_ARG(TechCommand, cmd),
                                  Q_ARG(uint16_t, mask),
                                  Q_ARG(QByteArray, data));
        return result;
    }
    return m_engine->executeGroupCommand(cmd, mask, data);
}

void CommunicationFacade::onEngineGroupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary)
{
    emit groupCommandCompleted(groupId, allSuccess, summary);
}

void CommunicationFacade::sendFUTransmit(uint16_t address, uint8_t period, const uint8_t fuData[3])
{
    // fuData is commonly stack-backed in the GUI/Lua caller. Copy it before
    // queueing work to the communication thread; capturing the pointer itself
    // is a use-after-scope race.
    const QByteArray dataCopy = fuData
        ? QByteArray(reinterpret_cast<const char*>(fuData), 3)
        : QByteArray();

    invokeInThread([this, address, period, dataCopy]() {
        const uint8_t* data = dataCopy.size() == 3
            ? reinterpret_cast<const uint8_t*>(dataCopy.constData())
            : nullptr;
        m_engine->sendFUTransmit(address, period, data);
    });
}

void CommunicationFacade::sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3])
{
    const QByteArray dataCopy = fuData
        ? QByteArray(reinterpret_cast<const char*>(fuData), 3)
        : QByteArray();

    invokeInThread([this, address, period, dataCopy]() {
        const uint8_t* data = dataCopy.size() == 3
            ? reinterpret_cast<const uint8_t*>(dataCopy.constData())
            : nullptr;
        m_engine->sendFUReceive(address, period, data);
    });
}

void CommunicationFacade::sendDataPackets(const QVector<DataPacket>& packets)
{
    invokeInThread([this, packets]() { m_engine->sendDataPackets(packets); });
}

PPBState CommunicationFacade::state() const
{
    if (QThread::currentThread() != thread()) {
        PPBState result = PPBState::Idle;
        QMetaObject::invokeMethod(const_cast<CommunicationFacade*>(this),
                                  [this, &result]() { result = m_engine->overallState(); },
                                  Qt::BlockingQueuedConnection);
        return result;
    }
    return m_engine->overallState();
}

bool CommunicationFacade::isBusy() const
{
    if (QThread::currentThread() != thread()) {
        bool result = false;
        QMetaObject::invokeMethod(const_cast<CommunicationFacade*>(this),
                                  [this, &result]() { result = m_engine->isBusy(); },
                                  Qt::BlockingQueuedConnection);
        return result;
    }
    return m_engine->isBusy();
}

void CommunicationFacade::onEngineStateChanged(uint16_t address, PPBState state)
{
    emit this->stateChanged(address, state);
}

void CommunicationFacade::onEngineCommandCompleted(bool success, const QString& message, TechCommand command)
{
    emit commandCompleted(success, message, command);
}

void CommunicationFacade::onEngineErrorOccurred(const QString& error)
{
    emit errorOccurred(error);
}

void CommunicationFacade::onEngineCommandDataParsed(uint16_t address, const QVariant& data, TechCommand command)
{
    emit commandDataParsed(address, data, command);
}

void CommunicationFacade::onEngineSentPacketsSaved(const QVector<DataPacket>& packets)
{
    emit sentPacketsSaved(packets);
}

void CommunicationFacade::onEngineReceivedPacketsSaved(const QVector<DataPacket>& packets)
{
    emit receivedPacketsSaved(packets);
}

void CommunicationFacade::onEngineClearPacketDataRequested()
{
    emit clearPacketDataRequested();
}

void CommunicationFacade::onEngineCommandProgress(int current, int total, TechCommand command)
{
    emit commandProgress(current, total, command);
}

void CommunicationFacade::onEngineBusyChanged(bool busy)
{
    emit busyChanged(busy);
}

void CommunicationFacade::setBridgeAddress(const QString &ip, quint16 port)
{
    invokeInThread([this, ip, port]() { m_engine->setBridgeAddress(ip, port); });
}

void CommunicationFacade::clearCommandQueue(uint16_t address)
{
    invokeInThread([this, address]() { m_engine->clearCommandQueue(address); });
}
