#ifndef SCENARIOENGINE_H
#define SCENARIOENGINE_H

#include <QObject>
#include <QTimer>
#include <sol/sol.hpp>
#include <functional>
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

   Q_INVOKABLE  bool loadScript(const QString &fileName);
   Q_INVOKABLE  bool execute();

public slots:
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

    // Вспомогательные методы для ожидания команд
    template<typename Func>
    bool waitForCommand(uint16_t address, Func&& commandLauncher, TechCommand expectedCmd, int timeoutMs = 10000);
    bool waitForFUCommand(uint16_t address, uint8_t fuCmd, std::function<void()> commandLauncher, int timeoutMs = 2000);

    sol::state lua;
    PPBController* m_controller;
    bool m_stopRequested;
};

// Определение шаблона (должно быть в заголовке)
template<typename Func>
bool ScenarioEngine::waitForCommand(uint16_t address, Func&& commandLauncher,
                                    TechCommand expectedCmd, int timeoutMs)
{
    if (m_stopRequested) return false;

    QEventLoop loop;
    bool success = false;
    bool timeout = false;

    QMetaObject::Connection conn;
    QTimer timer;
    timer.setSingleShot(true);
    timer.callOnTimeout([&] { timeout = true; loop.quit(); });

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
    loop.exec();

    QMetaObject::invokeMethod(m_controller, [&] { disconnect(conn); },
                              Qt::BlockingQueuedConnection);

    return success && !timeout && !m_stopRequested;
}

#endif // SCENARIOENGINE_H
