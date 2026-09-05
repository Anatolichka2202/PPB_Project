#ifndef SCENARIOENGINE_H
#define SCENARIOENGINE_H

#include <QObject>
#include <QTimer>
#include <sol/sol.hpp>
#include <functional>
#include <atomic>
#include "ppbprotocol.h"  // для TechCommand
#include "ppbcontrollerlib.h"
#include <QEventLoop>
class PPBController;

class ScenarioEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioEngine(PPBController* controller, QObject *parent = nullptr);
    ~ScenarioEngine();

   Q_INVOKABLE bool loadScript(const QString &fileName);
   Q_INVOKABLE bool execute();

   bool loadEmbeddedScript(const QString& name);

public slots:
    // Потокобезопасно: stop() может вызываться напрямую из GUI/controller
    // thread, пока Lua main() занят в scenario thread.
    void stop();

signals:
    void logMessage(const QString &msg);
    void errorOccurred(const QString &err);
    void finished(bool success);

private:
    // Lua-обёртки
    bool luaRequestStatus(uint16_t address);

    bool luaSendTC(uint16_t address);
    bool luaSetFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle);
    bool luaSetFUTransmit(uint16_t address, uint16_t duration, uint16_t dutyCycle);
    bool luaRequestVersion(uint16_t address);
    bool luaRequestChecksum(uint16_t address);
    bool luaRequestDroppedPackets(uint16_t address);
    bool luaRequestBER_T(uint16_t address);
    bool luaRequestBER_F(uint16_t address);
    bool luaRequestFabricNumber(uint16_t address);
    bool luaStartPRBS_M2S(uint16_t address);
    bool luaStartPRBS_S2M(uint16_t address);
    void luaSleep(int ms);

    bool luaSetFUTransmit(uint16_t address, uint16_t duration, uint8_t dutyCycle[3]);
    bool luaSetFUReceive(uint16_t address, uint16_t duration, uint8_t dutyCycle[3]);

    bool luaGeneratorAvailable();
    bool luaSetGeneratorFrequency(int channel, double freqHz);
    bool luaSetGeneratorAmplitude(int channel, double value, const std::string& unit);
    bool luaSetGeneratorOutput(int channel, bool enable);
    bool luaSetGeneratorWaveform(int channel, const std::string& wave);
    bool luaSetGeneratorDutyCycle(int channel, double percent);
    std::string luaGetGeneratorIdentity();

    bool loadScriptContent(const QString& content, const QString& scriptName);

    // Вспомогательные методы для ожидания команд
    template<typename Func>
    bool waitForCommand(uint16_t address, Func&& commandLauncher, TechCommand expectedCmd, int timeoutMs = 10000);
    bool waitForFUCommand(uint16_t address, uint8_t fuCmd, std::function<void()> commandLauncher, int timeoutMs = 2000);

    void luaLog(const std::string &msg); //логирование скриптов

    sol::state lua;
    PPBController* m_controller;
    std::atomic_bool m_stopRequested{false};
    QString m_scriptName;
};

// Определение шаблона (должно быть в заголовке)
template<typename Func>
bool ScenarioEngine::waitForCommand(uint16_t address, Func&& commandLauncher,
                                    TechCommand expectedCmd, int timeoutMs)
{
    Q_UNUSED(address)

    if (m_stopRequested.load(std::memory_order_acquire)) return false;

    QEventLoop loop;
    bool success = false;
    bool timeout = false;

    QMetaObject::Connection conn;
    QTimer timer;
    timer.setSingleShot(true);
    timer.callOnTimeout([&] { timeout = true; loop.quit(); });

    // stop() вызывается напрямую из другого потока и только меняет atomic flag.
    // Этот локальный таймер будит nested event loop, чтобы не ждать hardware
    // timeout до 10/30 секунд после нажатия "Остановить".
    QTimer stopPoll;
    stopPoll.setInterval(20);
    stopPoll.callOnTimeout([&] {
        if (m_stopRequested.load(std::memory_order_acquire)) {
            loop.quit();
        }
    });

    QMetaObject::invokeMethod(m_controller, [&] {
        conn = connect(m_controller, &PPBController::commandCompleted,
                       [&](bool s, const QString&, TechCommand cmd) {
                           if (cmd == expectedCmd) {
                               success = s;
                               loop.quit();
                           }
                       });
        commandLauncher();
    }, Qt::BlockingQueuedConnection);

    timer.start(timeoutMs);
    stopPoll.start();
    loop.exec();
    stopPoll.stop();

    QMetaObject::invokeMethod(m_controller, [&] { disconnect(conn); },
                              Qt::BlockingQueuedConnection);

    return success && !timeout && !m_stopRequested.load(std::memory_order_acquire);
}

#endif // SCENARIOENGINE_H
