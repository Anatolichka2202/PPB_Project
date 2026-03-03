#include <QCoreApplication>
#include <QSignalSpy>
#include <QDebug>
#include <QThread>
#include "communicationengine.h"
#include "protocoladapter.h"
#include "mockudpclient.h"
#include "ppbprotocol.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Создаём мок-клиент и адаптер
    MockUDPClient mockUdp;
    auto adapter = std::make_unique<ProtocolAdapter>();
    communicationengine engine(&mockUdp, std::move(adapter));

    // Устанавливаем адрес и порт (для отправки)
    engine.connectToPPB(0x1234, "127.0.0.1", 12345);

    // Сигналы для отслеживания
    QSignalSpy commandCompletedSpy(&engine, &communicationengine::commandCompleted);
    QSignalSpy commandDataParsedSpy(&engine, &communicationengine::commandDataParsed);

    // ========== Тест 1: VERS с успешным ответом и данными ==========
    qDebug() << "\n=== Test 1: VERS with data ===";
    engine.executeCommand(TechCommand::VERS, 0x1234);

    // Формируем ответ ППБ: заголовок (address, status) + 2 байта данных
    TUResponseHeader header;
    header.address = 0x1234;
    header.status = 0x00; // OK
    QByteArray payload;
    payload.append(static_cast<char>(0x12));
    payload.append(static_cast<char>(0x34));
    QByteArray response(reinterpret_cast<const char*>(&header), sizeof(header));
    response.append(payload);

    // Эмулируем получение
    mockUdp.simulateReceive(response);

    // Даём время на обработку
    QCoreApplication::processEvents();
    QThread::msleep(100);
    QCoreApplication::processEvents();

    // Проверяем результаты
    if (commandCompletedSpy.count() > 0) {
        QList<QVariant> args = commandCompletedSpy.takeFirst();
        bool success = args[0].toBool();
        QString message = args[1].toString();
        TechCommand cmd = args[2].value<TechCommand>();
        qDebug() << "commandCompleted: success=" << success << "message=" << message << "cmd=" << (int)cmd;
    } else {
        qDebug() << "No commandCompleted signal";
    }

    if (commandDataParsedSpy.count() > 0) {
        QList<QVariant> args = commandDataParsedSpy.takeFirst();
        uint16_t addr = args[0].toUInt();
        QVariant data = args[1];
        TechCommand cmd = args[2].value<TechCommand>();
        qDebug() << "commandDataParsed: addr=" << addr << "cmd=" << (int)cmd;
        if (data.type() == QVariant::ByteArray) {
            QByteArray ba = data.toByteArray();
            qDebug() << "  data (bytes):" << ba.toHex();
        } else {
            qDebug() << "  data type:" << data.typeName();
        }
    } else {
        qDebug() << "No commandDataParsed signal";
    }

    // ========== Тест 2: Ошибочный ответ ==========
    qDebug() << "\n=== Test 2: Error response ===";
    commandCompletedSpy.clear();
    commandDataParsedSpy.clear();

    engine.executeCommand(TechCommand::VERS, 0x1234);

    header.status = 0x01; // ошибка
    response = QByteArray(reinterpret_cast<const char*>(&header), sizeof(header));
    mockUdp.simulateReceive(response);

    QCoreApplication::processEvents();
    QThread::msleep(100);
    QCoreApplication::processEvents();

    if (commandCompletedSpy.count() > 0) {
        QList<QVariant> args = commandCompletedSpy.takeFirst();
        bool success = args[0].toBool();
        QString message = args[1].toString();
        TechCommand cmd = args[2].value<TechCommand>();
        qDebug() << "commandCompleted: success=" << success << "message=" << message << "cmd=" << (int)cmd;
    } else {
        qDebug() << "No commandCompleted signal";
    }

    if (commandDataParsedSpy.count() == 0) {
        qDebug() << "No data parsed (as expected)";
    }

    // ========== Тест 3: Ответ бриджа ==========
    qDebug() << "\n=== Test 3: Bridge response ===";
    commandCompletedSpy.clear();
    commandDataParsedSpy.clear();

    // Отправляем ФУ команду (например, FU Transmit)
    engine.sendFUTransmit(0x1234);

    // Формируем ответ бриджа
    BridgeResponse bridgeResp;
    bridgeResp.address = 0x1234;
    bridgeResp.command = 0; // команда transmit
    bridgeResp.status = 1;  // OK
    QByteArray bridgeResponse(reinterpret_cast<const char*>(&bridgeResp), sizeof(bridgeResp));
    mockUdp.simulateReceive(bridgeResponse);

    QCoreApplication::processEvents();
    QThread::msleep(100);
    QCoreApplication::processEvents();

    if (commandCompletedSpy.count() > 0) {
        QList<QVariant> args = commandCompletedSpy.takeFirst();
        bool success = args[0].toBool();
        QString message = args[1].toString();
        TechCommand cmd = args[2].value<TechCommand>();
        qDebug() << "commandCompleted: success=" << success << "message=" << message << "cmd=" << (int)cmd;
    } else {
        qDebug() << "No commandCompleted signal";
    }

    return 0;
}
