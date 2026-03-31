#ifndef PACKETANALYZER_ADAPTER_H
#define PACKETANALYZER_ADAPTER_H

#pragma once

#include "../common/include/packetanalyzer_interface.h"
#include "packetanalyzer.h"

class PacketAnalyzerAdapter : public PacketAnalyzerInterface {
    Q_OBJECT
private:
    PacketAnalyzer m_analyzer;

public:
    explicit PacketAnalyzerAdapter(QObject* parent = nullptr)
        : PacketAnalyzerInterface(parent) {
        connect(&m_analyzer, &PacketAnalyzer::analysisStarted,
                this, &PacketAnalyzerAdapter::analysisStarted);
        connect(&m_analyzer, &PacketAnalyzer::analysisProgress,
                this, &PacketAnalyzerAdapter::analysisProgress);
        connect(&m_analyzer, &PacketAnalyzer::analysisComplete,
                [this](const PacketAnalyzer::AnalysisResult& result) {
                    emit analysisComplete(result.toString());
                    QVariantMap details;
                    details["totalSent"] = result.totalSent;
                    details["totalReceived"] = result.totalReceived;
                    details["lostPackets"] = result.lostPackets;
                    details["ber"] = result.ber;
                    emit detailedResultsReady(details);
                });
    }

    void addSentPackets(const QVector<DataPacket>& packets) override {
        m_analyzer.addSentPackets(packets);
    }

    void addReceivedPackets(const QVector<DataPacket>& packets) override {
        m_analyzer.addReceivedPackets(packets);
    }

    void clear() override {
        m_analyzer.clear();
    }

    void analyze() override {
        m_analyzer.analyze();
    }

    void setCheckCRC(bool check) override {
        m_analyzer.setCheckCRC(check);
    }

    void setMaxReorderingWindow(int window) override {
        m_analyzer.setMaxReorderingWindow(window);
    }

    int sentCount() const override {
        return m_analyzer.sentCount();
    }

    int receivedCount() const override {
        return m_analyzer.receivedCount();
    }
};

#endif // PACKETANALYZER_ADAPTER_H
