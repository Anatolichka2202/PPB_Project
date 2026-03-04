#include "lanInterface.h"
#include <QThread>
#include <QDebug>

LanInterface::LanInterface(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_isConnected(false)
    , m_connectTimeout(5000)
    , m_responseTimeout(3000)
{
    connect(m_socket, &QTcpSocket::connected, this, &LanInterface::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &LanInterface::onSocketDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &LanInterface::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &LanInterface::onSocketReadyRead);
}

LanInterface::~LanInterface()
{
    disconnect();
}

bool LanInterface::connectToHost(const QString &host, quint16 port, int timeoutMs)
{
    if (m_isConnected)
        disconnect();

    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(timeoutMs > 0 ? timeoutMs : m_connectTimeout)) {
        QString errStr = m_socket->errorString();
        emit errorOccurred(tr("Connection failed: %1").arg(errStr));
        return false;
    }
    // connected signal will set m_isConnected
    return true;
}

void LanInterface::disconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->waitForDisconnected(1000);
    }
    m_isConnected = false;
    m_readBuffer.clear();
    emit disconnected();
}

bool LanInterface::isConnected() const
{
    return m_isConnected;
}

bool LanInterface::sendScpiCommand(const QString &command)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred("Socket not connected");
        return false;
    }


    if (!m_isConnected) {
        emit errorOccurred(tr("Not connected"));
        return false;
    }

    QByteArray data = command.toLatin1() + '\r\n';
    qint64 written = m_socket->write(data);
    if (written != data.size()) {
        emit errorOccurred(tr("Failed to write all data (written %1 of %2)").arg(written).arg(data.size()));
        return false;
    }
    m_socket->flush();
    qDebug() << "[LAN] Отправлено:" << command;
    return true;
}

QString LanInterface::queryScpiCommand(const QString &command, int timeoutMs, int retries)
{
    clearReadBuffer();
    QString response;
    for (int attempt = 0; attempt <= retries; ++attempt) {
        if (!sendScpiCommand(command))
            return QString();

        response = waitForResponse(timeoutMs > 0 ? timeoutMs : m_responseTimeout);
        if (!response.isEmpty()) {
            qDebug() << "[LAN] Получено:" << response;
            break;
        }
        qDebug() << "[LAN] Попытка" << attempt+1 << "неудачна, повтор...";
        QThread::msleep(100);
    }
    return response;
}

void LanInterface::clearReadBuffer()
{
    m_readBuffer.clear();
    // также можно прочитать все ожидающие данные из сокета
    if (m_socket->bytesAvailable() > 0)
        m_readBuffer = m_socket->readAll();
}

void LanInterface::onSocketConnected()
{
    m_isConnected = true;
    emit connected();
}

void LanInterface::onSocketDisconnected()
{
    m_isConnected = false;
    m_readBuffer.clear();
    emit disconnected();
}

void LanInterface::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    emit errorOccurred(m_socket->errorString());
}

void LanInterface::onSocketReadyRead()
{
    m_readBuffer += m_socket->readAll();
}

QString LanInterface::waitForResponse(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    // Сначала проверим, есть ли уже в буфере полная строка
    int idx = m_readBuffer.indexOf('\n');
    if (idx == -1) idx = m_readBuffer.indexOf('\r');
    if (idx >= 0) {
        QByteArray line = m_readBuffer.left(idx).trimmed();
        m_readBuffer.remove(0, idx + 1);
        return QString::fromLatin1(line);
    }

    // Ждём новые данные
    while (timer.elapsed() < timeoutMs) {
        if (m_socket->waitForReadyRead(100)) { // ждём до 100 мс
            // Проверяем, появилась ли строка
            idx = m_readBuffer.indexOf('\n');
            if (idx == -1) idx = m_readBuffer.indexOf('\r');
            if (idx >= 0) {
                QByteArray line = m_readBuffer.left(idx).trimmed();
                m_readBuffer.remove(0, idx + 1);
                return QString::fromLatin1(line);
            }
        } else {
            // Таймаут waitForReadyRead, но могли прийти данные без уведомления? Вряд ли.
            // Проверим буфер ещё раз
            idx = m_readBuffer.indexOf('\n');
            if (idx == -1) idx = m_readBuffer.indexOf('\r');
            if (idx >= 0) {
                QByteArray line = m_readBuffer.left(idx).trimmed();
                m_readBuffer.remove(0, idx + 1);
                return QString::fromLatin1(line);
            }
        }
    }

    // По истечении таймаута всё равно проверим, нет ли данных в буфере
    idx = m_readBuffer.indexOf('\n');
    if (idx == -1) idx = m_readBuffer.indexOf('\r');
    if (idx >= 0) {
        QByteArray line = m_readBuffer.left(idx).trimmed();
        m_readBuffer.remove(0, idx + 1);
        return QString::fromLatin1(line);
    }

    // Если ничего нет, возвращаем пустую строку (ошибка)
    return QString();
}
