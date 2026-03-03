#ifndef PACKETANALYZER_INTERFACE_H
#define PACKETANALYZER_INTERFACE_H

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include "icontroller.h" // для DataPacket

class PacketAnalyzerInterface : public QObject {
    Q_OBJECT
public:
    explicit PacketAnalyzerInterface(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PacketAnalyzerInterface() = default;

    virtual void addSentPackets(const QVector<DataPacket>& packets) = 0;
    virtual void addReceivedPackets(const QVector<DataPacket>& packets) = 0;
    virtual void clear() = 0;
    virtual void analyze() = 0;
    virtual void setCheckCRC(bool check) = 0;
    virtual void setMaxReorderingWindow(int window) = 0;
    virtual int sentCount() const = 0;
    virtual int receivedCount() const = 0;

signals:
    void analysisStarted();
    void analysisProgress(int percent);
    void analysisComplete(const QString& resultSummary);
    void detailedResultsReady(const QVariantMap& results);
};

#endif // PACKETANALYZER_INTERFACE_H
