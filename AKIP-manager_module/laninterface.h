#ifndef LANINTERFACE_H
#define LANINTERFACE_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QTimer>
#include <QDebug>

/**
 * @brief Класс для работы с LAN-подключением к прибору по протоколу SCPI.
 *
 * Обеспечивает установку соединения, отправку команд, получение ответов
 * с поддержкой таймаутов и повторных попыток.
 */
class LanInterface : public QObject
{
    Q_OBJECT
public:
    explicit LanInterface(QObject *parent = nullptr);
    ~LanInterface();

    // ==================== Управление подключением ====================
    bool connectToHost(const QString &host, quint16 port = 5025, int timeoutMs = 5000);
    void disconnect();
    bool isConnected() const;

    // ==================== Отправка команд ====================
    bool sendScpiCommand(const QString &command);
    QString queryScpiCommand(const QString &command, int timeoutMs = 3000, int retries = 1);

    // ==================== Очистка буфера ====================
    void clearReadBuffer();

    // ==================== Настройка таймаутов ====================
    void setConnectTimeout(int ms) { m_connectTimeout = ms; }
    void setResponseTimeout(int ms) { m_responseTimeout = ms; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private slots:
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();

private:
    QTcpSocket *m_socket;
    bool m_isConnected;
    int m_connectTimeout;
    int m_responseTimeout;
    QByteArray m_readBuffer;

    QString waitForResponse(int timeoutMs);
};

#endif // LANINTERFACE_H
