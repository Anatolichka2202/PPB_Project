#include "udpclient.h"
#include "../comm_dependencies.h"
#include <QNetworkDatagram>
#include <QDebug>
#include <QThread>
#include <QVariant>

UDPClient::UDPClient(QObject* parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_isBound(false)
    , m_boundPort(0)
{
    // Сокет будет создан позже в initializeInThread()
}

UDPClient::~UDPClient()
{
    LOG_TECH_NETWORK("UDPClient destructor");

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
}

void UDPClient::initializeInThread()
{
    LOG_TECH_THREAD(QString("initializeInThread - thread: %1")
                        .arg((qulonglong)QThread::currentThreadId()));

    if (m_socket) {
        LOG_TECH_NETWORK("already initialized");
        return;
    }

    try {
        setupSocket();
        LOG_TECH_NETWORK("successfully initialized in thread");
        emit initialized();
    } catch (const std::exception& e) {
        LOG_UI_ALERT(QString("UDPClient initialization error: %1").arg(e.what()));
        emit errorOccurred(QString("UDPClient initialization error: %1").arg(e.what()));
    }
}

void UDPClient::setupSocket()
{
    LOG_TECH_NETWORK("setupSocket");

    m_socket = new QUdpSocket(this);

    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 65536);
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 65536);

    if (!bind(101)) {
        throw std::runtime_error("Failed to bind to port 101");
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &UDPClient::readPendingDatagrams);

    connect(m_socket, &QUdpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError error) {
                QString errorMsg = QString("Socket error: %1 - %2")
                .arg(error)
                    .arg(m_socket->errorString());
                LOG_UI_ALERT(errorMsg);
                emit errorOccurred(errorMsg);
            });

    LOG_TECH_NETWORK(QString("socket configured and bound to port %1").arg(m_boundPort));
}

bool UDPClient::bind(quint16 port)
{
    if (!m_socket) {
        LOG_ERROR(LogCategory::TECH_NETWORK, "bind - socket not created");
        return false;
    }

    if (m_isBound) {
        LOG_TECH_NETWORK(QString("already bound to port %1").arg(m_boundPort));
        return true;
    }

    LOG_TECH_NETWORK(QString("bind - attempting port %1").arg(port));

    QHostAddress addr;
    QByteArray ipEnv = qgetenv("UDP_BIND_IP");
    if (!ipEnv.isEmpty()) {
        addr = QHostAddress(QString::fromLocal8Bit(ipEnv));
        if (addr.isNull()) {
            qWarning() << "Invalid UDP_BIND_IP, using default 192.168.0.246";
            addr = QHostAddress("192.168.0.246");
        }
    } else {
        addr = QHostAddress(QHostAddress::Any);
    }

    if (m_socket->bind (addr, port)) {
        m_isBound = true;
        m_boundPort = m_socket->localPort();
        m_boundAddress = m_socket->localAddress();

        LOG_UI_OPERATION(QString("Bound to port %1 on %2")
                             .arg(m_boundPort).arg(m_boundAddress.toString()));

        emit bindingChanged(true);
        return true;


    } else {
        QString errorMsg = QString("Failed to bind to port %1: %2")
        .arg(port)
            .arg(m_socket->errorString());
        LOG_UI_ALERT(errorMsg);
        emit errorOccurred(errorMsg);
        return false;
    }
}

void UDPClient::unbind()
{
    LOG_TECH_NETWORK("unbind");

    if (m_socket && m_isBound) {
        m_socket->close();
        m_isBound = false;
        m_boundPort = 0;
        m_boundAddress.clear();

        emit bindingChanged(false);
        LOG_TECH_NETWORK("unbound from port");
    }
}

bool UDPClient::isBound() const
{
    return m_isBound && m_socket != nullptr &&
           m_socket->state() == QAbstractSocket::BoundState;
}

qint64 UDPClient::sendTo(const QByteArray& data, const QString& address, quint16 port)
{
    LOG_TECH_DEBUG(QString("sendTo: %1:%2, size=%3 bytes")
                       .arg(address).arg(port).arg(data.size()));

    if (!m_socket) {
        LOG_UI_ALERT("sendTo - socket not initialized");
        emit errorOccurred("Socket not initialized");
        return -1;
    }

    if (!m_isBound) {
        LOG_UI_ALERT("sendTo - socket not bound");
        emit errorOccurred("Socket not bound");
        return -1;
    }

    QHostAddress hostAddress;
    if (!hostAddress.setAddress(address)) {
        LOG_ERROR(LogCategory::TECH_NETWORK, "sendTo - invalid address: " + address);
        emit errorOccurred("Invalid address: " + address);
        return -1;
    }

    qint64 bytesSent = m_socket->writeDatagram(data, hostAddress, port);

    if (bytesSent == -1) {
        QString errorMsg = QString("Error sending to %1:%2: %3")
        .arg(address)
            .arg(port)
            .arg(m_socket->errorString());
        LOG_UI_ALERT(errorMsg);
        emit errorOccurred(errorMsg);
    } else {
        LOG_TECH_NETWORK(QString("Sent %1 bytes to %2:%3")
                             .arg(bytesSent).arg(address).arg(port));
        LOG_UI_DATA("Raw data sent (hex): " + data.toHex(' '));
        emit dataSent(bytesSent);
    }

    return bytesSent;
}

qint64 UDPClient::sendBroadcast(const QByteArray& data, quint16 port)
{
    LOG_TECH_DEBUG(QString("sendBroadcast: port=%1, size=%2 bytes")
                       .arg(port).arg(data.size()));

    if (!m_socket) {
        LOG_UI_ALERT("sendBroadcast - socket not initialized");
        emit errorOccurred("Socket not initialized");
        return -1;
    }

    if (!m_isBound) {
        LOG_UI_ALERT("sendBroadcast - socket not bound");
        emit errorOccurred("Socket not bound");
        return -1;
    }

    bool wasBroadcastEnabled = m_socket->socketOption(QAbstractSocket::MulticastTtlOption).toBool();
    if (!wasBroadcastEnabled) {
        m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, QVariant(1));
    }

    qint64 bytesSent = m_socket->writeDatagram(data, QHostAddress::Broadcast, port);

    if (!wasBroadcastEnabled) {
        m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, QVariant(0));
    }

    if (bytesSent == -1) {
        QString errorMsg = QString("Error broadcasting to port %1: %2")
        .arg(port)
            .arg(m_socket->errorString());
        LOG_UI_ALERT(errorMsg);
        emit errorOccurred(errorMsg);
    } else {
        LOG_TECH_NETWORK(QString("Broadcast %1 bytes to port %2")
                             .arg(bytesSent).arg(port));
        emit dataSent(bytesSent);
    }

    return bytesSent;
}

void UDPClient::readPendingDatagrams()
{
    if (!m_socket) return;

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();

        if (datagram.isValid()) {
            QByteArray data = datagram.data();
            QHostAddress sender = datagram.senderAddress();
            quint16 port = datagram.senderPort();

            emit dataReceived(data, sender, port);

            LOG_UI_DATA(QString("UDP received %1 bytes from %2:%3")
                            .arg(data.size()).arg(sender.toString()).arg(port));

            if (data.size() <= 64) {
                LOG_UI_DATA("Raw data receive (hex): " + data.toHex(' '));
            } else {
                LOG_UI_DATA("Raw data (first 64 bytes): " + data.left(64).toHex(' '));
            }

        } else {
            LOG_TECH_DEBUG("Received invalid datagram");
        }
    }
}
