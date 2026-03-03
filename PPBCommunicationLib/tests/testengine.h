#ifndef TEST_ENGINE_H
#define TEST_ENGINE_H

#include <QObject>
#include <QThread>
#include <memory>

// Forward declarations
class MockUDPClient;
class communicationengine;

class TestEngine : public QObject {
    Q_OBJECT

public:
    explicit TestEngine(QObject *parent = nullptr);

private slots:
    void init();               // выполняется перед каждым тестом
    void cleanup();            // после каждого теста

    // Тестовые сценарии
    void testConnectAndStatus();
    void testTuCommandVers();
    void testTuCommandError();
    void testFuCommandTransmit();
    void testFuCommandReceive();
    void testCommandQueue();
    void testTimeout();
    void testBusySignal();
    void testMultiAddress();
    void testThreadSafety();

private:
    MockUDPClient* m_mockUdp;
    communicationengine* m_engine;
    QThread* m_engineThread;
};

#endif // TEST_ENGINE_H
