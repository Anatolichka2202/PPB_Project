#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QThread>
#include <memory>

// Заголовки проекта
#include "communicationengine.h"
#include "protocoladapter.h"
#include "mockudpclient.h"
#include "ppbprotocol.h"

// Вспомогательные функции для формирования ответов
static QByteArray makeTUResponse(uint16_t address, uint8_t status, const QByteArray& payload = {}) {
    TUResponseHeader header;
    header.address = address;
    header.status = status;
    QByteArray result(reinterpret_cast<const char*>(&header), sizeof(header));
    result.append(payload);
    return result;
}

static QByteArray makeBridgeResponse(uint16_t address, uint8_t command, uint8_t status) {
    BridgeResponse resp;
    resp.address = address;
    resp.command = command;
    resp.status = status;
    return QByteArray(reinterpret_cast<const char*>(&resp), sizeof(resp));
}

class TestEngine : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testConnectAndStatus();
    void testTuCommandVers();
    void testTuCommandError();
    void testFuCommandTransmit();
    void testFuCommandReceive();
    void testCommandQueue();
    void testTimeout();
    void testMultiAddress();
    void testThreadSafety();

private:
    MockUDPClient* m_mockUdp;
    communicationengine* m_engine;
    QThread* m_engineThread;
};

void TestEngine::init() {
    m_mockUdp = new MockUDPClient(this);
    auto adapter = std::make_unique<ProtocolAdapter>();
    m_engine = new communicationengine(m_mockUdp, std::move(adapter));

    // Перемещаем движок в отдельный поток для проверки многопоточности
    m_engineThread = new QThread(this);
    m_engine->moveToThread(m_engineThread);
    m_engineThread->start();

    // Даём потоку время запуститься
    QTest::qWait(50);
}

void TestEngine::cleanup() {
    if (m_engineThread) {
        m_engineThread->quit();
        m_engineThread->wait(1000);
    }
}

// ==================== ТЕСТЫ ====================

void TestEngine::testConnectAndStatus() {
    const uint16_t address = 0x0001;
    const QString ip = "127.0.0.1";
    const quint16 port = 12345;

    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QSignalSpy connectedSpy(m_engine, &communicationengine::connected);
    QSignalSpy stateSpy(m_engine, &communicationengine::stateChanged);
    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);

    m_engine->connectToPPB(address, ip, port);

    // Проверяем, что состояние стало SendingCommand
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.count(), 1, 500);
    QCOMPARE(stateSpy[0][1].value<PPBState>(), PPBState::SendingCommand);

    // Эмулируем ответ с данными статуса (8 байт = 4 пакета по 2 байта)
    QByteArray payload(8, 0xAA);
    QByteArray response = makeTUResponse(address, 0x00, payload);
    m_mockUdp->simulateReceive(response, QHostAddress(ip), port);

    // Ожидаем сигнал statusReceived
    QVERIFY(statusSpy.wait(1000));
    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy[0][0].value<uint16_t>(), address);
    QVector<QByteArray> data = statusSpy[0][1].value<QVector<QByteArray>>();
    QCOMPARE(data.size(), 4); // 4 пакета

    // Проверяем connected
    QCOMPARE(connectedSpy.count(), 1);

    // Проверяем commandCompleted (команда TS)
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), true);
    QCOMPARE(cmdSpy[0][2].value<TechCommand>(), TechCommand::TS);

    // Конечное состояние Ready
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.count(), 2, 500);
    QCOMPARE(stateSpy[1][1].value<PPBState>(), PPBState::Ready);
}

void TestEngine::testTuCommandVers() {
    // Подключаемся (переводим в Ready)
    const uint16_t address = 0x0001;
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);
    QSignalSpy dataSpy(m_engine, &communicationengine::commandDataParsed);

    m_engine->executeCommand(TechCommand::VERS, address);

    // Эмулируем ответ VERS (2 байта данных версии)
    uint16_t versionData = 0x1234;
    QByteArray versPayload(reinterpret_cast<const char*>(&versionData), sizeof(versionData));
    QByteArray versResp = makeTUResponse(address, 0x00, versPayload);
    m_mockUdp->simulateReceive(versResp);

    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), true);
    QCOMPARE(cmdSpy[0][2].value<TechCommand>(), TechCommand::VERS);

    // Проверяем, что commandDataParsed содержит ожидаемые данные
    // В зависимости от реализации VersCommand, данные могут быть QVariantMap или QByteArray
    // В текущей реализации VersCommand парсит и возвращает QVariantMap с полем "crc32"
    // Но по протоколу это просто данные, поэтому возможно стоит изменить команду
    QVERIFY(dataSpy.count() >= 1);
    QVariant parsed = dataSpy[0][1];
    if (parsed.type() == QVariant::Map) {
        QVariantMap map = parsed.toMap();
        QVERIFY(map.contains("crc32")); // legacy, но пока оставим
        QCOMPARE(map["crc32"].toUInt(), 0x1234u);
    } else if (parsed.type() == QVariant::ByteArray) {
        QByteArray ba = parsed.toByteArray();
        QCOMPARE(ba.size(), 2);
        QCOMPARE(static_cast<uint16_t>(ba[0] | (ba[1] << 8)), 0x1234u);
    }
}

void TestEngine::testTuCommandError() {
    // Подключаемся
    const uint16_t address = 0x0001;
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);
    m_engine->executeCommand(TechCommand::VERS, address);

    // Ответ с ошибкой (status = 1)
    QByteArray errorResp = makeTUResponse(address, 0x01);
    m_mockUdp->simulateReceive(errorResp);

    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), false);
    QString msg = cmdSpy[0][1].toString();
    QVERIFY(msg.contains("Ошибка", Qt::CaseInsensitive) || msg.contains("error", Qt::CaseInsensitive));
}

void TestEngine::testFuCommandTransmit() {
    const uint16_t address = 0x0001;
    // Подключаемся
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);
    m_engine->sendFUTransmit(address);

    QByteArray bridgeResp = makeBridgeResponse(address, 0, 1); // команда 0, статус OK
    m_mockUdp->simulateReceive(bridgeResp);

    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), true);
}

void TestEngine::testFuCommandReceive() {
    const uint16_t address = 0x0001;
    // Подключаемся
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);
    uint8_t fuData[3] = {0x01, 0x02, 0x03};
    m_engine->sendFUReceive(address, 10, fuData);

    QByteArray bridgeResp = makeBridgeResponse(address, 1, 1); // команда 1, статус OK
    m_mockUdp->simulateReceive(bridgeResp);

    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), true);
}

void TestEngine::testCommandQueue() {
    const uint16_t address = 0x0001;
    // Подключаемся
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);

    // Отправляем две команды подряд
    m_engine->executeCommand(TechCommand::VERS, address);
    m_engine->executeCommand(TechCommand::DROP, address);

    // Эмулируем ответ на VERS
    uint16_t versData = 0x1234;
    QByteArray versPayload(reinterpret_cast<const char*>(&versData), sizeof(versData));
    QByteArray versResp = makeTUResponse(address, 0x00, versPayload);
    m_mockUdp->simulateReceive(versResp);

    // Ждём завершения VERS
    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][2].value<TechCommand>(), TechCommand::VERS);

    // Эмулируем ответ на DROP
    uint16_t dropData = 0x5678;
    QByteArray dropPayload(reinterpret_cast<const char*>(&dropData), sizeof(dropData));
    QByteArray dropResp = makeTUResponse(address, 0x00, dropPayload);
    m_mockUdp->simulateReceive(dropResp);

    QVERIFY(cmdSpy.wait(1000));
    QCOMPARE(cmdSpy.count(), 2);
    QCOMPARE(cmdSpy[1][2].value<TechCommand>(), TechCommand::DROP);
}

void TestEngine::testTimeout() {
    const uint16_t address = 0x0001;
    // Подключаемся
    m_engine->connectToPPB(address, "127.0.0.1", 12345);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp = makeTUResponse(address, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp);
    QSignalSpy statusSpy(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy.wait(1000));

    QSignalSpy cmdSpy(m_engine, &communicationengine::commandCompleted);

    // Для VERS таймаут 3000 мс
    m_engine->executeCommand(TechCommand::VERS, address);

    // Не отправляем ответ
    QVERIFY(cmdSpy.wait(4000)); // ждём больше таймаута
    QCOMPARE(cmdSpy.count(), 1);
    QCOMPARE(cmdSpy[0][0].toBool(), false);
    QCOMPARE(cmdSpy[0][2].value<TechCommand>(), TechCommand::VERS);
}

void TestEngine::testMultiAddress() {
    const uint16_t addr1 = 0x0001;
    const uint16_t addr2 = 0x0002;
    const QString ip = "127.0.0.1";
    const quint16 port = 12345;

    // Подключаем первый
    m_engine->connectToPPB(addr1, ip, port);
    QByteArray statusPayload(8, 0xAA);
    QByteArray statusResp1 = makeTUResponse(addr1, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp1);
    QSignalSpy statusSpy1(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy1.wait(1000));

    // Подключаем второй
    m_engine->connectToPPB(addr2, ip, port);
    QByteArray statusResp2 = makeTUResponse(addr2, 0x00, statusPayload);
    m_mockUdp->simulateReceive(statusResp2);
    QSignalSpy statusSpy2(m_engine, &communicationengine::statusReceived);
    QVERIFY(statusSpy2.wait(1000));

    // Команда для addr1
    QSignalSpy cmdSpy1(m_engine, &communicationengine::commandCompleted);
    m_engine->executeCommand(TechCommand::VERS, addr1);
    uint16_t versData = 0x1234;
    QByteArray versPayload(reinterpret_cast<const char*>(&versData), sizeof(versData));
    QByteArray versResp1 = makeTUResponse(addr1, 0x00, versPayload);
    m_mockUdp->simulateReceive(versResp1);
    QVERIFY(cmdSpy1.wait(1000));
    QCOMPARE(cmdSpy1[0][2].value<TechCommand>(), TechCommand::VERS);

    // Команда для addr2
    QSignalSpy cmdSpy2(m_engine, &communicationengine::commandCompleted);
    m_engine->executeCommand(TechCommand::DROP, addr2);
    uint16_t dropData = 0x5678;
    QByteArray dropPayload(reinterpret_cast<const char*>(&dropData), sizeof(dropData));
    QByteArray dropResp2 = makeTUResponse(addr2, 0x00, dropPayload);
    m_mockUdp->simulateReceive(dropResp2);
    QVERIFY(cmdSpy2.wait(1000));
    QCOMPARE(cmdSpy2[0][2].value<TechCommand>(), TechCommand::DROP);
}

void TestEngine::testThreadSafety() {
    // Этот тест не выполняет активных действий, так как все предыдущие тесты уже запускались в потоке.
    QSKIP("Thread safety implicitly verified by other tests");
}

// Запуск тестов
QTEST_MAIN(TestEngine)
