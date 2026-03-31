// testanalyzer.cpp
#include "testanalyzer.h"
#include <iostream>

TestAnalyzer::TestAnalyzer(QObject *parent)
    : QObject(parent)
{
}

DataPacket TestAnalyzer::createTestPacket(uint8_t data1, uint8_t data2)
{
    DataPacket packet;
    packet.data[0] = data1;
    packet.data[1] = data2;
    return packet;
}

void TestAnalyzer::printResult(const PacketAnalyzer::AnalysisResult &result)
{
    std::cout << "\n" << result.toString().toStdString() << std::endl;

    if (!result.errorDetails.isEmpty()) {
        std::cout << "\nДетали ошибок:" << std::endl;
        for (const auto &detail : result.errorDetails) {
            if (detail.isLost || detail.bitErrors > 0) {
                std::cout << "  Пакет " << detail.index << ": "
                          << detail.sentData.toStdString() << " -> "
                          << detail.receivedData.toStdString()
                          << " Ошибки: " << detail.bitErrors
                          << (detail.isLost ? " LOST" : "")
                          << std::endl;
            }
        }
    }
}

void TestAnalyzer::testPerfectMatch()
{
    std::cout << "\n=== Тест 1: Идеальное совпадение ===" << std::endl;

    QVector<DataPacket> sent;
    QVector<DataPacket> received;

    for (int i = 0; i < 256; i++) {
        DataPacket packet = createTestPacket(i & 0xFF, (255 - i) & 0xFF);
        sent.append(packet);
        received.append(packet);
    }

    auto result = m_analyzer.analyze(sent, received);
    printResult(result);
}

void TestAnalyzer::testPacketLoss()
{
    std::cout << "\n=== Тест 2: Потеря пакетов ===" << std::endl;

    QVector<DataPacket> sent;
    QVector<DataPacket> received;

    for (int i = 0; i < 256; i++) {
        DataPacket packet = createTestPacket(i & 0xFF, (255 - i) & 0xFF);
        sent.append(packet);
        if (i % 10 != 0) {
            received.append(packet);
        }
    }

    auto result = m_analyzer.analyze(sent, received);
    printResult(result);
}

void TestAnalyzer::testBitErrors()
{
    std::cout << "\n=== Тест 3: Битовые ошибки ===" << std::endl;

    QVector<DataPacket> sent;
    QVector<DataPacket> received;

    for (int i = 0; i < 100; i++) {
        DataPacket sentPacket = createTestPacket(i & 0xFF, (255 - i) & 0xFF);
        sent.append(sentPacket);

        DataPacket receivedPacket = sentPacket;
        if (i % 5 == 0) {
            receivedPacket.data[0] ^= 0x01;
            receivedPacket.data[1] ^= 0x02;
        }
        received.append(receivedPacket);
    }

    auto result = m_analyzer.analyze(sent, received);
    printResult(result);
}

void TestAnalyzer::testExtraPackets()
{
    std::cout << "\n=== Тест 4: Лишние пакеты ===" << std::endl;

    QVector<DataPacket> sent;
    QVector<DataPacket> received;

    for (int i = 0; i < 50; i++) {
        sent.append(createTestPacket(i & 0xFF, (255 - i) & 0xFF));
    }

    for (int i = 0; i < 60; i++) {
        received.append(createTestPacket(i & 0xFF, (255 - i) & 0xFF));
    }

    auto result = m_analyzer.analyze(sent, received);
    printResult(result);
}

void TestAnalyzer::runAllTests()
{
    std::cout << "Запуск всех тестов анализатора..." << std::endl;

    testPerfectMatch();
    testPacketLoss();
    testBitErrors();
    testExtraPackets();

    std::cout << "\n=== Все тесты пройдены успешно! ===" << std::endl;
}
