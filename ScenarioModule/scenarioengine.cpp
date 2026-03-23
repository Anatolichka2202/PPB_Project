#include "scenarioengine.h"
#include "ppbcontrollerlib.h"
#include <QFile>
#include <QTextStream>
#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QThread>

ScenarioEngine::ScenarioEngine(PPBController* controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_stopRequested(false)
{
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    lua.set_function("log", [this](const std::string &msg) {
        emit logMessage(QString::fromStdString(msg));
    });

    lua.set_function("requestStatus",  [this](uint16_t a) { return luaRequestStatus(a); });
    lua.set_function("sendTC",         [this](uint16_t a) { return luaSendTC(a); });
    lua.set_function("setFUReceive",   [this](uint16_t a, uint16_t d, uint16_t c) { return luaSetFUReceive(a, d, c); });
    lua.set_function("setFUTransmit",  [this](uint16_t a, uint16_t d, uint16_t c) { return luaSetFUTransmit(a, d, c); });
    lua.set_function("requestVersion", [this](uint16_t a) { return luaRequestVersion(a); });
    lua.set_function("requestChecksum",[this](uint16_t a) { return luaRequestChecksum(a); });
    lua.set_function("requestDropped", [this](uint16_t a) { return luaRequestDroppedPackets(a); });
    lua.set_function("requestBER_T",   [this](uint16_t a) { return luaRequestBER_T(a); });
    lua.set_function("requestBER_F",   [this](uint16_t a) { return luaRequestBER_F(a); });
    lua.set_function("requestFabricNumber", [this](uint16_t a) { return luaRequestFabricNumber(a); });
    lua.set_function("startPRBS_M2S",  [this](uint16_t a) { return luaStartPRBS_M2S(a); });
    lua.set_function("startPRBS_S2M",  [this](uint16_t a) { return luaStartPRBS_S2M(a); });
    lua.set_function("sleep",          [this](int ms) { luaSleep(ms); });

    lua["engine"] = this;
}

ScenarioEngine::~ScenarioEngine()
{
    stop();
}

void ScenarioEngine::stop()
{
    m_stopRequested = true;
}

bool ScenarioEngine::loadEmbeddedScript(const QString& name) {
    QString path = QString(":/scenario/scripts/%1.lua").arg(name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open embedded script: " + path);
        return false;
    }
    QString content = QString::fromUtf8(file.readAll());
    return loadScript(content);
}

bool ScenarioEngine::loadScript(const QString &fileName)
{
    qDebug() << "Loading script from:" << fileName;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file:" << fileName;
        emit errorOccurred("Cannot open file: " + fileName);
        return false;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    qDebug() << "Script content length:" << content.length();
    // Удалить BOM, если есть
    if (content.startsWith(QChar(0xFEFF))) {
        content = content.mid(1);
        qDebug() << "Removed BOM";
    }
    try {
        lua.script(content.toStdString());
        qDebug() << "Script loaded successfully";
        // Проверяем наличие main
        sol::function main = lua["main"];
        if (main.valid()) {
            qDebug() << "main function found in Lua state";
        } else {
            qDebug() << "main function NOT found in Lua state";
            // Выводим содержимое глобальной таблицы для диагностики (осторожно, может быть много)
            sol::table globals = lua["_G"];
            for (const auto& kv : globals) {
                sol::object key = kv.first;
                if (key.is<std::string>()) {
                    qDebug() << "Global:" << key.as<std::string>().c_str();
                }
            }
        }
    } catch (const sol::error &e) {
        qDebug() << "Lua error:" << e.what();
        emit errorOccurred(QString("Lua error: %1").arg(e.what()));
        return false;
    }
    return true;
}
bool ScenarioEngine::execute()
{
    qDebug() << "ScenarioEngine::execute() started";
    m_stopRequested = false;
    try {
        sol::function main = lua["main"];
        qDebug() << "main valid?" << main.valid();
        if (!main.valid()) {
            emit errorOccurred("Script does not contain 'main' function");
            return false;
        }
        qDebug() << "Calling main()...";
        main();
        qDebug() << "main() finished";
        emit finished(true);
    } catch (const sol::error &e) {
        qDebug() << "Execution error:" << e.what();
        emit errorOccurred(QString("Execution error: %1").arg(e.what()));
        emit finished(false);
        return false;
    }
    return true;
}

bool ScenarioEngine::waitForFUCommand(uint16_t address, uint8_t fuCmd,
                                      std::function<void()> commandLauncher,
                                      int timeoutMs)
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
        conn = connect(m_controller, &PPBController::fuCommandCompleted,
                       [&](uint16_t addr, uint8_t cmd, bool s, const QString&) {
                           if (addr == address && cmd == fuCmd) {
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

bool ScenarioEngine::luaRequestStatus(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestStatus(address); },
                          TechCommand::TS, 10000);
}

bool ScenarioEngine::luaSendTC(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->sendTC(address); },
                          TechCommand::TC, 5000);
}

bool ScenarioEngine::luaSetFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle)
{
    return waitForFUCommand(address, 0,
                            [this, address, duration, dutyCycle] {
                                m_controller->setFUReceive(address, duration, dutyCycle);
                            }, 2000);
}

bool ScenarioEngine::luaSetFUTransmit(uint16_t address, uint16_t duration, uint16_t dutyCycle)
{
    return waitForFUCommand(address, 1,
                            [this, address, duration, dutyCycle] {
                                m_controller->setFUTransmit(address, duration, dutyCycle);
                            }, 2000);
}

bool ScenarioEngine::luaRequestVersion(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestVersion(address); },
                          TechCommand::VERS, 5000);
}

bool ScenarioEngine::luaRequestChecksum(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestChecksum(address); },
                          TechCommand::CHECKSUM, 5000);
}

bool ScenarioEngine::luaRequestDroppedPackets(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestDroppedPackets(address); },
                          TechCommand::DROP, 5000);
}

bool ScenarioEngine::luaRequestBER_T(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestBER_T(address); },
                          TechCommand::BER_T, 5000);
}

bool ScenarioEngine::luaRequestBER_F(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestBER_F(address); },
                          TechCommand::BER_F, 5000);
}

bool ScenarioEngine::luaRequestFabricNumber(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->requestFabricNumber(address); },
                          TechCommand::Factory_Number, 5000);
}

bool ScenarioEngine::luaStartPRBS_M2S(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->startPRBS_M2S(address); },
                          TechCommand::PRBS_M2S, 30000);
}

bool ScenarioEngine::luaStartPRBS_S2M(uint16_t address)
{
    return waitForCommand(address,
                          [this, address] { m_controller->startPRBS_S2M(address); },
                          TechCommand::PRBS_S2M, 30000);
}

void ScenarioEngine::luaSleep(int ms)
{
    if (m_stopRequested) return;
    QThread::msleep(ms);
}
