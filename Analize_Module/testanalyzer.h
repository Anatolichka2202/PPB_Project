// testanalyzer.h
#ifndef TESTANALYZER_H
#define TESTANALYZER_H

#include "packetanalyzer.h"
#include <QObject>

class TestAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit TestAnalyzer(QObject *parent = nullptr);
    void runAllTests();

private:
    DataPacket createTestPacket(uint8_t data1, uint8_t data2);
    void printResult(const PacketAnalyzer::AnalysisResult &result);

    void testPerfectMatch();        // Идеальное совпадение
    void testPacketLoss();          // Потеря пакетов
    void testBitErrors();           // Битовые ошибки
    void testExtraPackets();        // Лишние пакеты

    PacketAnalyzer m_analyzer;
};

#endif // TESTANALYZER_H
