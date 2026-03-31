#include "packetanalyzer.h"
#include <QDebug>
#include <cmath>

PacketAnalyzer::PacketAnalyzer(QObject *parent)
    : QObject(parent)
{
}

void PacketAnalyzer::addSentPackets(const QVector<DataPacket> &packets)
{
    for (const DataPacket &packet : packets) {
        PacketInfo info(packet, m_sentSequenceCounter++);
        m_sentPackets.append(info);
    }
}

void PacketAnalyzer::addReceivedPackets(const QVector<DataPacket> &packets)
{
    for (const DataPacket &packet : packets) {
        PacketInfo info(packet, m_receivedSequenceCounter++);
        m_receivedPackets.append(info);
    }
}

void PacketAnalyzer::clear()
{
    m_sentPackets.clear();
    m_receivedPackets.clear();
    m_sentSequenceCounter = 0;
    m_receivedSequenceCounter = 0;
}

PacketAnalyzer::AnalysisResult PacketAnalyzer::analyze()
{
    emit analysisStarted();

    QElapsedTimer timer;
    timer.start();

    AnalysisResult result;
    result.totalSent = m_sentPackets.size();
    result.totalReceived = m_receivedPackets.size();

    int processed = 0;
    int compareCount = qMin(result.totalSent, result.totalReceived);

    for (int i = 0; i < compareCount; ++i) {
        const PacketInfo &sentInfo = m_sentPackets[i];
        const PacketInfo &receivedInfo = m_receivedPackets[i];

        // Считаем битовые ошибки
        int bitErrors = countBitErrors(sentInfo.packet, receivedInfo.packet);

        if (bitErrors == 0) {
            result.validPackets++;
        } else {
            result.bitErrors += bitErrors;
            result.bitErrorIndices.append(i);
        }

        result.totalBitsCompared += 16; // 2 байта = 16 бит

        // Детали ошибок для отладки
        AnalysisResult::PacketErrorDetail detail;
        detail.index = i;
        detail.isLost = false;
        detail.bitErrors = bitErrors;
        detail.sentData = packetToString(sentInfo.packet);
        detail.receivedData = packetToString(receivedInfo.packet);
        result.errorDetails.append(detail);

        processed++;
        if (result.totalSent > 0) {
            emit analysisProgress((processed * 100) / result.totalSent);
        }
    }

    // Потерянные пакеты (если отправлено больше, чем получено)
    if (result.totalSent > result.totalReceived) {
        result.lostPackets = result.totalSent - result.totalReceived;
        for (int i = compareCount; i < result.totalSent; ++i) {
            result.lostPacketIndices.append(i);

            AnalysisResult::PacketErrorDetail detail;
            detail.index = i;
            detail.isLost = true;
            detail.bitErrors = 0;
            detail.sentData = packetToString(m_sentPackets[i].packet);
            detail.receivedData = "LOST";
            result.errorDetails.append(detail);
        }
    }

    // Лишние пакеты (если получено больше, чем отправлено)
    int extraPackets = result.totalReceived - result.totalSent;
    if (extraPackets > 0) {
        for (int i = compareCount; i < result.totalReceived; ++i) {
            AnalysisResult::PacketErrorDetail detail;
            detail.index = i;
            detail.isLost = false;
            detail.bitErrors = 0;
            detail.sentData = "NOT SENT";
            detail.receivedData = packetToString(m_receivedPackets[i].packet);
            result.errorDetails.append(detail);
        }
    }

    // Рассчитываем rates
    if (result.totalSent > 0) {
        result.packetLossRate = static_cast<double>(result.lostPackets) / result.totalSent;
        if (result.totalBitsCompared > 0) {
            result.ber = static_cast<double>(result.bitErrors) / result.totalBitsCompared;
        }
    }

    result.analysisTimeMs = timer.elapsed();

    emit analysisComplete(result);
    return result;
}

PacketAnalyzer::AnalysisResult PacketAnalyzer::analyze(
    const QVector<DataPacket> &sent,
    const QVector<DataPacket> &received)
{
    clear();
    addSentPackets(sent);
    addReceivedPackets(received);
    return analyze();
}

int PacketAnalyzer::countBitErrors(const DataPacket &sent, const DataPacket &received) const
{
    int errors = 0;
    uint8_t xor0 = sent.data[0] ^ received.data[0];
    uint8_t xor1 = sent.data[1] ^ received.data[1];

    while (xor0) { errors += xor0 & 1; xor0 >>= 1; }
    while (xor1) { errors += xor1 & 1; xor1 >>= 1; }

    return errors;
}

QString PacketAnalyzer::packetToString(const DataPacket &packet) const
{
    return QString("[%1 %2]")
    .arg(packet.data[0], 2, 16, QChar('0'))
        .arg(packet.data[1], 2, 16, QChar('0'));
}

QString PacketAnalyzer::AnalysisResult::toString() const
{
    return QString("Sent: %1, Received: %2, Lost: %3, Bit errors: %4, BER: %5, Valid: %6")
    .arg(totalSent)
        .arg(totalReceived)
        .arg(lostPackets)
        .arg(bitErrors)
        .arg(ber, 0, 'e', 6)
        .arg(validPackets);
}
