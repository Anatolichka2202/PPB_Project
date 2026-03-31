#ifndef PACKETANALYZER_H
#define PACKETANALYZER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QElapsedTimer>
#include <QString>
#include "ppbprotocol.h"

class PacketAnalyzer : public QObject
{
    Q_OBJECT

public:
    struct AnalysisResult {
        double ber = 0.0;
        double packetLossRate = 0.0;
        int totalSent = 0;
        int totalReceived = 0;
        int validPackets = 0;
        int lostPackets = 0;
        int bitErrors = 0;
        int totalBitsCompared = 0;
        QVector<int> lostPacketIndices;
        QVector<int> bitErrorIndices;
        qint64 analysisTimeMs = 0;

        struct PacketErrorDetail {
            int index;
            bool isLost;
            int bitErrors;
            QString sentData;
            QString receivedData;
        };
        QVector<PacketErrorDetail> errorDetails;

        QString toString() const;
    };

    explicit PacketAnalyzer(QObject *parent = nullptr);

    void addSentPackets(const QVector<DataPacket> &packets);
    void addReceivedPackets(const QVector<DataPacket> &packets);
    void clear();

    AnalysisResult analyze();
    AnalysisResult analyze(const QVector<DataPacket> &sent,
                           const QVector<DataPacket> &received);

    // Методы, требуемые интерфейсом PacketAnalyzerInterface
    void setCheckCRC(bool) {}          // не используется, но нужен для интерфейса
    void setMaxReorderingWindow(int) {} // не используется
    int sentCount() const { return m_sentPackets.size(); }
    int receivedCount() const { return m_receivedPackets.size(); }

signals:
    void analysisStarted();
    void analysisProgress(int percent);
    void analysisComplete(const AnalysisResult &result);
    void errorOccurred(const QString &error);

private:
    struct PacketInfo {
        DataPacket packet;
        int sequenceNumber;
        PacketInfo() : sequenceNumber(-1) {}
        PacketInfo(const DataPacket &p, int seq) : packet(p), sequenceNumber(seq) {}
    };

    int countBitErrors(const DataPacket &sent, const DataPacket &received) const;
    QString packetToString(const DataPacket &packet) const;

    QVector<PacketInfo> m_sentPackets;
    QVector<PacketInfo> m_receivedPackets;
    int m_sentSequenceCounter = 0;
    int m_receivedSequenceCounter = 0;
};

#endif // PACKETANALYZER_H
