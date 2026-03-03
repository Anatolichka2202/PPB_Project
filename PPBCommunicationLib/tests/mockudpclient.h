#ifndef MOCKUDPCLIENT_H
#define MOCKUDPCLIENT_H

#include "../src/udpclient.h"
#include <QByteArray>
#include <QHostAddress>

class MockUDPClient : public UDPClient {
    Q_OBJECT
public:
    explicit MockUDPClient(QObject* parent = nullptr) : UDPClient(parent) {}

    // Переопределяем методы, чтобы они ничего не делали
    bool bind(quint16 = 101) override { return true; }
    void unbind() override {}
    bool isBound() const override { return true; }
    qint64 sendTo(const QByteArray&, const QString&, quint16) override { return 0; }
    qint64 sendBroadcast(const QByteArray&, quint16) override { return 0; }

    // Метод для имитации получения данных
    void simulateReceive(const QByteArray& data, const QHostAddress& sender = QHostAddress("127.0.0.1"), quint16 port = 1234) {
        emit dataReceived(data, sender, port);
    }
};

#endif // MOCKUDPCLIENT_H
