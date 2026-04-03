#include "firmwareupdater.h"
#include <QFile>
#include <QTextStream>
#include <QtEndian>
#include <QDebug>

QVector<QByteArray> FirmwareUpdater::parseHexToDataBlocks(const QString &hexFilePath)
{
    QVector<QByteArray> dataBlocks;
    QFile file(hexFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open HEX file:" << hexFilePath;
        return dataBlocks;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (!line.startsWith(':')) continue;

        if (line.length() < 11) continue;
        bool ok;
        int byteCount = line.mid(1, 2).toInt(&ok, 16);
        if (!ok || byteCount <= 0) continue;
        int recordType = line.mid(7, 2).toInt(&ok, 16);
        if (!ok) continue;

        if (recordType == 0) {
            QString dataHex = line.mid(9, byteCount * 2);
            QByteArray data = QByteArray::fromHex(dataHex.toLatin1());
            int offset = 0;
            while (offset + 16 <= data.size()) {
                dataBlocks.append(data.mid(offset, 16));
                offset += 16;
            }
            if (offset < data.size()) {
                dataBlocks.append(data.mid(offset));
            }
        } else if (recordType == 1) {
            break;
        }
    }
    file.close();
    return dataBlocks;
}

QVector<QByteArray> FirmwareUpdater::buildVolumePayloads(const QVector<QByteArray> &dataBlocks)
{
    QVector<QByteArray> payloads;
    const int BLOCKS_PER_VOLUME = 64; // 64 * 16 = 1024 байта
    int numVolumes = (dataBlocks.size() + BLOCKS_PER_VOLUME - 1) / BLOCKS_PER_VOLUME;

    for (int volIdx = 0; volIdx < numVolumes; ++volIdx) {
        int startIdx = volIdx * BLOCKS_PER_VOLUME;
        int endIdx = qMin(startIdx + BLOCKS_PER_VOLUME, dataBlocks.size());
        QByteArray volumeData;
        for (int i = startIdx; i < endIdx; ++i) {
            volumeData.append(dataBlocks[i]);
        }
        payloads.append(volumeData);
    }
    return payloads;
}
