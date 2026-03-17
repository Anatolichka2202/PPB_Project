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
    // Соединяем сигналы движка с внутренними слотами
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
    // Добавляем новые соединения
    connect(m_engine, &communicationengine::commandProgress,
            this, &CommunicationFacade::onEngineCommandProgress);
    connect(m_engine, &communicationengine::busyChanged,
            this, &CommunicationFacade::onEngineBusyChanged);
}

CommunicationFacade::CommunicationFacade(QObject* parent)
    : ICommunication(parent)
    , m_ownsUdpClient(true)
    , m_udpClient(new UDPClient(this))  // создаём как дочерний объект
    , m_engine(nullptr)  // пока не создаём, нужно дождаться инициализации?
{
    // В этом конструкторе мы ещё не можем создать движок, так как для него нужен адаптер.
    // Создадим адаптер по умолчанию и движок позже, после того как переместим в поток.
    // Но поскольку мы ещё не в потоке, отложим создание до initialize().
    // Альтернатива: создать адаптер и движок прямо здесь, но тогда они будут в основном потоке.
    // Лучше всё создавать в потоке в initialize().
    // Пока только сохраняем указатель на UDPClient (он уже дочерний).
}

CommunicationFacade::~CommunicationFacade()
{
    // Если мы владеем UDPClient, он удалится автоматически как дочерний объект.
    // Движок тоже дочерний, удалится.
}

void CommunicationFacade::initialize()
{
    // Этот метод должен вызываться, когда объект уже находится в своём рабочем потоке.
    // Создаём адаптер и движок.
    if (m_engine) {
        qWarning() << "CommunicationFacade already initialized";
        return;
    }

    // Создаём адаптер протокола (можно заменить на фабрику, если нужно)
    auto adapter = std::make_unique<ProtocolAdapter>();

    // Создаём движок
    m_engine = new communicationengine(m_udpClient, std::move(adapter), this);

    // Повторяем соединения (как в другом конструкторе)
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

    // Инициализируем UDPClient в его потоке
    QMetaObject::invokeMethod(m_udpClient, "initializeInThread", Qt::QueuedConnection);

    // Когда UDPClient инициализируется, он пошлёт сигнал initialized, который мы должны пробросить.
    // Но у нас пока нет соединения с сигналом UDPClient::initialized. Добавим его.
    connect(m_udpClient, &UDPClient::initialized, this, &CommunicationFacade::initialized);
}

// ... остальные методы интерфейса с проверкой потока

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

void CommunicationFacade::sendFUTransmit(uint16_t address)
{
    invokeInThread([this, address]() { m_engine->sendFUTransmit(address); });
}

void CommunicationFacade::sendFUReceive(uint16_t address, uint8_t period, const uint8_t fuData[3])
{
    invokeInThread([this, address, period, fuData]() {
        m_engine->sendFUReceive(address, period, fuData);
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

// Приватные слоты для проброса сигналов

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


