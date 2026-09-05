#include "firmwareupdater.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <limits>

namespace {

bool parseHexRecord(const QString& line,
                    int lineNumber,
                    QByteArray& record,
                    quint16& recordAddress,
                    quint8& recordType,
                    QByteArray& recordData)
{
    if (!line.startsWith(':')) {
        qWarning() << "Invalid Intel HEX record at line" << lineNumber
                   << "- missing ':' prefix";
        return false;
    }

    const QString hex = line.mid(1);
    if (hex.size() < 10 || (hex.size() % 2) != 0) {
        qWarning() << "Invalid Intel HEX record length at line" << lineNumber;
        return false;
    }

    static const QRegularExpression hexOnly(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!hexOnly.match(hex).hasMatch()) {
        qWarning() << "Invalid non-hex character at line" << lineNumber;
        return false;
    }

    record = QByteArray::fromHex(hex.toLatin1());
    if (record.size() < 5) {
        qWarning() << "Intel HEX record is too short at line" << lineNumber;
        return false;
    }

    const int byteCount = static_cast<quint8>(record.at(0));
    const int expectedSize = byteCount + 5; // count + address(2) + type + data + checksum
    if (record.size() != expectedSize) {
        qWarning() << "Intel HEX byte count mismatch at line" << lineNumber
                   << "expected bytes:" << expectedSize
                   << "actual bytes:" << record.size();
        return false;
    }

    quint8 checksum = 0;
    for (char byte : record) {
        checksum = static_cast<quint8>(checksum + static_cast<quint8>(byte));
    }
    if (checksum != 0) {
        qWarning() << "Intel HEX checksum error at line" << lineNumber;
        return false;
    }

    recordAddress =
        (static_cast<quint16>(static_cast<quint8>(record.at(1))) << 8) |
        static_cast<quint16>(static_cast<quint8>(record.at(2)));
    recordType = static_cast<quint8>(record.at(3));
    recordData = record.mid(4, byteCount);
    return true;
}

bool requireControlRecord(const QByteArray& recordData,
                          int expectedDataSize,
                          quint16 recordAddress,
                          int lineNumber,
                          const char* recordName)
{
    if (recordAddress != 0 || recordData.size() != expectedDataSize) {
        qWarning() << "Invalid" << recordName << "record at line" << lineNumber
                   << "address:" << QStringLiteral("0x%1").arg(recordAddress, 4, 16, QLatin1Char('0'))
                   << "data bytes:" << recordData.size();
        return false;
    }
    return true;
}

} // namespace

QVector<QByteArray> FirmwareUpdater::parseHexToDataBlocks(const QString &hexFilePath)
{
    QVector<QByteArray> dataBlocks;
    QFile file(hexFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open HEX file:" << hexFilePath;
        return dataBlocks;
    }

    QTextStream in(&file);
    bool eofSeen = false;
    bool dataSeen = false;
    quint32 addressBase = 0;
    quint64 expectedNextAddress = 0;
    int lineNumber = 0;

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        ++lineNumber;

        if (line.isEmpty()) {
            continue;
        }

        if (eofSeen) {
            qWarning() << "Unexpected data after Intel HEX EOF record at line" << lineNumber;
            dataBlocks.clear();
            return dataBlocks;
        }

        QByteArray record;
        QByteArray recordData;
        quint16 recordAddress = 0;
        quint8 recordType = 0;
        if (!parseHexRecord(line, lineNumber, record, recordAddress, recordType, recordData)) {
            dataBlocks.clear();
            return dataBlocks;
        }

        switch (recordType) {
        case 0x00: { // Data
            const quint64 absoluteAddress =
                static_cast<quint64>(addressBase) + static_cast<quint64>(recordAddress);
            const quint64 endAddress = absoluteAddress + static_cast<quint64>(recordData.size());

            // The current PPB VOLUME protocol carries only a linear byte stream;
            // no absolute flash address is transmitted. Therefore a HEX image is
            // representable only when all data records form one contiguous range.
            if (dataSeen && absoluteAddress != expectedNextAddress) {
                const char* kind = absoluteAddress > expectedNextAddress
                    ? "gap"
                    : "overlap/out-of-order";
                qWarning() << "Intel HEX" << kind << "at line" << lineNumber
                           << "expected address:"
                           << QStringLiteral("0x%1").arg(expectedNextAddress, 0, 16)
                           << "record address:"
                           << QStringLiteral("0x%1").arg(absoluteAddress, 0, 16);
                dataBlocks.clear();
                return dataBlocks;
            }

            if (endAddress > static_cast<quint64>(std::numeric_limits<quint32>::max()) + 1ULL) {
                qWarning() << "Intel HEX data address exceeds 32-bit address space at line"
                           << lineNumber;
                dataBlocks.clear();
                return dataBlocks;
            }

            if (!recordData.isEmpty()) {
                int offset = 0;
                while (offset < recordData.size()) {
                    const int chunkSize = qMin(16, recordData.size() - offset);
                    dataBlocks.append(recordData.mid(offset, chunkSize));
                    offset += chunkSize;
                }
                dataSeen = true;
                expectedNextAddress = endAddress;
            }
            break;
        }

        case 0x01: // End Of File
            if (!requireControlRecord(recordData, 0, recordAddress, lineNumber, "EOF")) {
                dataBlocks.clear();
                return dataBlocks;
            }
            eofSeen = true;
            break;

        case 0x02: { // Extended Segment Address
            if (!requireControlRecord(recordData, 2, recordAddress, lineNumber,
                                      "Extended Segment Address")) {
                dataBlocks.clear();
                return dataBlocks;
            }
            const quint16 segment =
                (static_cast<quint16>(static_cast<quint8>(recordData.at(0))) << 8) |
                static_cast<quint16>(static_cast<quint8>(recordData.at(1)));
            addressBase = static_cast<quint32>(segment) << 4;
            break;
        }

        case 0x03: // Start Segment Address
            if (!requireControlRecord(recordData, 4, recordAddress, lineNumber,
                                      "Start Segment Address")) {
                dataBlocks.clear();
                return dataBlocks;
            }
            break;

        case 0x04: { // Extended Linear Address
            if (!requireControlRecord(recordData, 2, recordAddress, lineNumber,
                                      "Extended Linear Address")) {
                dataBlocks.clear();
                return dataBlocks;
            }
            const quint16 upper =
                (static_cast<quint16>(static_cast<quint8>(recordData.at(0))) << 8) |
                static_cast<quint16>(static_cast<quint8>(recordData.at(1)));
            addressBase = static_cast<quint32>(upper) << 16;
            break;
        }

        case 0x05: // Start Linear Address
            if (!requireControlRecord(recordData, 4, recordAddress, lineNumber,
                                      "Start Linear Address")) {
                dataBlocks.clear();
                return dataBlocks;
            }
            break;

        default:
            qWarning() << "Unsupported Intel HEX record type"
                       << QStringLiteral("0x%1").arg(recordType, 2, 16, QLatin1Char('0'))
                       << "at line" << lineNumber;
            dataBlocks.clear();
            return dataBlocks;
        }
    }

    if (!eofSeen) {
        qWarning() << "Intel HEX EOF record is missing:" << hexFilePath;
        dataBlocks.clear();
        return dataBlocks;
    }

    if (!dataSeen || dataBlocks.isEmpty()) {
        qWarning() << "Intel HEX contains no data records:" << hexFilePath;
        dataBlocks.clear();
    }

    return dataBlocks;
}

QVector<QByteArray> FirmwareUpdater::buildVolumePayloads(const QVector<QByteArray> &dataBlocks)
{
    QVector<QByteArray> payloads;
    const int BLOCKS_PER_VOLUME = 64; // legacy layout: up to 64 chunks per VOLUME
    const int numVolumes = (dataBlocks.size() + BLOCKS_PER_VOLUME - 1) / BLOCKS_PER_VOLUME;

    for (int volIdx = 0; volIdx < numVolumes; ++volIdx) {
        const int startIdx = volIdx * BLOCKS_PER_VOLUME;
        const int endIdx = qMin(startIdx + BLOCKS_PER_VOLUME, dataBlocks.size());
        QByteArray volumeData;
        for (int i = startIdx; i < endIdx; ++i) {
            volumeData.append(dataBlocks[i]);
        }
        payloads.append(volumeData);
    }
    return payloads;
}
