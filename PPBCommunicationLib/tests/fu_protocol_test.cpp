#include "../../common/include/ppbprotocol.h"
#include "packetbuilder.h"
#include "protocoladapter.h"
#include <QtTest/QtTest>

class FuProtocolTest : public QObject {
    Q_OBJECT
private slots:
    void packetModesAndAllParameterBytes();
    void bridgeRecognizesNewMode();
    void bridgeRejectsUnknownMode();
};

void FuProtocolTest::packetModesAndAllParameterBytes()
{
    static_assert(sizeof(BaseRequest) == 8, "FU request must stay 8 bytes");
    const uint8_t fuData[3] = {0x12, 0x34, 0x2D};
    ProtocolAdapter adapter;
    const struct {
        FuCommand mode;
        const char* expectedHex;
    } cases[] = {
        { FuCommand::RECEIVE,          "000200011212342d" },
        { FuCommand::TRANSMIT,         "000201011212342d" },
        { FuCommand::RECEIVE_TRANSMIT, "000202011212342d" }
    };

    for (const auto& c : cases) {
        const QByteArray packet = adapter.buildFURequest(
            0x0002, static_cast<uint8_t>(c.mode), 0x12, fuData);
        QCOMPARE(packet, QByteArray::fromHex(c.expectedHex));
    }

    const uint8_t smallData[3] = {0x00, 0x64, 0x1E};
    QCOMPARE(PacketBuilder::createFURequest(
                 0x0001, static_cast<uint8_t>(FuCommand::RECEIVE_TRANSMIT),
                 0x00, smallData),
             QByteArray::fromHex("000102010000641e"));
}

void FuProtocolTest::bridgeRecognizesNewMode()
{
    ProtocolAdapter adapter;
    for (quint8 command = 0; command <= 2; ++command) {
        QByteArray data = QByteArray::fromHex("00010001");
        data[2] = static_cast<char>(command);
        ProtocolEvent event;
        QVERIFY(adapter.parseBridgeResponse(data, event));
        QCOMPARE(event.command, command);
        QCOMPARE(event.status, quint8(1));
        QVERIFY(event.type == ProtocolEvent::BridgeResponse);
    }
    ProtocolEvent failure;
    QVERIFY(adapter.parseBridgeResponse(QByteArray::fromHex("00010200"), failure));
    QCOMPARE(failure.status, quint8(0));
}

void FuProtocolTest::bridgeRejectsUnknownMode()
{
    ProtocolAdapter adapter;
    ProtocolEvent event;
    QVERIFY(!adapter.parseBridgeResponse(QByteArray::fromHex("00010301"), event));
}

QTEST_GUILESS_MAIN(FuProtocolTest)
#include "fu_protocol_test.moc"
