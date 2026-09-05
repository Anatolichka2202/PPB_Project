#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "firmwareupdater.h"

namespace {

QString makeRecord(quint16 address, quint8 type, const QByteArray& data)
{
    QByteArray bytes;
    bytes.append(static_cast<char>(data.size()));
    bytes.append(static_cast<char>((address >> 8) & 0xFF));
    bytes.append(static_cast<char>(address & 0xFF));
    bytes.append(static_cast<char>(type));
    bytes.append(data);

    quint8 sum = 0;
    for (char byte : bytes) {
        sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
    }
    bytes.append(static_cast<char>(static_cast<quint8>(0u - sum)));

    return QStringLiteral(":") + QString::fromLatin1(bytes.toHex().toUpper());
}

QString writeHex(const QStringList& lines, QTemporaryDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("firmware.hex"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return {};
    }

    for (const QString& line : lines) {
        file.write(line.toLatin1());
        file.write("\n");
    }
    file.close();
    return path;
}

QByteArray flatten(const QVector<QByteArray>& blocks)
{
    QByteArray result;
    for (const QByteArray& block : blocks) {
        result.append(block);
    }
    return result;
}

} // namespace

class FirmwareUpdaterTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptsContiguousImage();
    void acceptsContiguousImageAcrossExtendedLinearBoundary();
    void rejectsGap();
    void rejectsOverlapOrOutOfOrder();
    void rejectsBadChecksum();
    void rejectsMissingEof();
};

void FirmwareUpdaterTest::acceptsContiguousImage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QByteArray first = QByteArray::fromHex("00112233445566778899AABBCCDDEEFF");
    const QByteArray second = QByteArray::fromHex("10203040");
    const QString path = writeHex({
        makeRecord(0x1000, 0x00, first),
        makeRecord(0x1010, 0x00, second),
        makeRecord(0x0000, 0x01, {})
    }, dir);

    QVERIFY(!path.isEmpty());
    const auto blocks = FirmwareUpdater::parseHexToDataBlocks(path);
    QVERIFY(!blocks.isEmpty());
    QCOMPARE(flatten(blocks), first + second);
}

void FirmwareUpdaterTest::acceptsContiguousImageAcrossExtendedLinearBoundary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QByteArray tailOfBank(16, static_cast<char>(0xA5));
    const QByteArray nextBank = QByteArray::fromHex("DEADBEEF");
    const QString path = writeHex({
        makeRecord(0x0000, 0x04, QByteArray::fromHex("0000")),
        makeRecord(0xFFF0, 0x00, tailOfBank),
        makeRecord(0x0000, 0x04, QByteArray::fromHex("0001")),
        makeRecord(0x0000, 0x00, nextBank),
        makeRecord(0x0000, 0x01, {})
    }, dir);

    QVERIFY(!path.isEmpty());
    const auto blocks = FirmwareUpdater::parseHexToDataBlocks(path);
    QVERIFY(!blocks.isEmpty());
    QCOMPARE(flatten(blocks), tailOfBank + nextBank);
}

void FirmwareUpdaterTest::rejectsGap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeHex({
        makeRecord(0x1000, 0x00, QByteArray::fromHex("01020304")),
        makeRecord(0x1008, 0x00, QByteArray::fromHex("05060708")),
        makeRecord(0x0000, 0x01, {})
    }, dir);

    QVERIFY(FirmwareUpdater::parseHexToDataBlocks(path).isEmpty());
}

void FirmwareUpdaterTest::rejectsOverlapOrOutOfOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeHex({
        makeRecord(0x1000, 0x00, QByteArray::fromHex("01020304")),
        makeRecord(0x1002, 0x00, QByteArray::fromHex("05060708")),
        makeRecord(0x0000, 0x01, {})
    }, dir);

    QVERIFY(FirmwareUpdater::parseHexToDataBlocks(path).isEmpty());
}

void FirmwareUpdaterTest::rejectsBadChecksum()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString bad = makeRecord(0x1000, 0x00, QByteArray::fromHex("01020304"));
    QVERIFY(bad.size() >= 2);
    bad[bad.size() - 1] = (bad.back() == QLatin1Char('0')) ? QLatin1Char('1') : QLatin1Char('0');

    const QString path = writeHex({
        bad,
        makeRecord(0x0000, 0x01, {})
    }, dir);

    QVERIFY(FirmwareUpdater::parseHexToDataBlocks(path).isEmpty());
}

void FirmwareUpdaterTest::rejectsMissingEof()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeHex({
        makeRecord(0x1000, 0x00, QByteArray::fromHex("01020304"))
    }, dir);

    QVERIFY(FirmwareUpdater::parseHexToDataBlocks(path).isEmpty());
}

QTEST_APPLESS_MAIN(FirmwareUpdaterTest)
#include "firmwareupdater_test.moc"
