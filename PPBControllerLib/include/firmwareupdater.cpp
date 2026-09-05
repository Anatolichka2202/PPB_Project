#include "firmwareupdater.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

bool parseHexRecord(const QString& line,
                    int lineNumber,
                    QByteArray& record,
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

    recordType = static_cast<quint8>(record.at(3));
    recordData = record.mid(4, byteCount);
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
        quint8 recordType = 0;
        if (!parseHexRecord(line, lineNumber, record, recordType, recordData)) {
            dataBlocks.clear();
            return dataBlocks;
        }

        switch (recordType) {
        case 0x00: { // Data record. Preserve existing behaviour: concatenate data records.
            int offset = 0;
            while (offset < recordData.size()) {
                const int chunkSize = qMin(16, recordData.size() - offset);
                dataBlocks.append(recordData.mid(offset, chunkSize));
                offset += chunkSize;
            }
            break;
        }
        case 0x01: // End Of File
            if (!recordData.isEmpty()) {
                qWarning() << "Invalid Intel HEX EOF payload at line" << lineNumber;
                dataBlocks.clear();
                return dataBlocks;
            }
            eofSeen = true;
            break;
        case 0x02: // Extended Segment Address
        case 0x03: // Start Segment Address
        case 0x04: // Extended Linear Address
        case 0x05: // Start Linear Address
            // These records are validated above but intentionally do not alter the
            // legacy payload layout. The PPB bootloader currently receives a linear
            // stream of data bytes rather than host-side absolute addresses.
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

    if (dataBlocks.isEmpty()) {
        qWarning() << "Intel HEX contains no data records:" << hexFilePath;
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
