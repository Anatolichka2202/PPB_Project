#ifndef FIRMWAREUPDATER_H
#define FIRMWAREUPDATER_H

#include <QVector>
#include <QByteArray>
#include <QString>

class FirmwareUpdater
{
public:
    static QVector<QByteArray> parseHexToDataBlocks(const QString &hexFilePath);
    static QVector<QByteArray> buildVolumePayloads(const QVector<QByteArray> &dataBlocks);
};

#endif
