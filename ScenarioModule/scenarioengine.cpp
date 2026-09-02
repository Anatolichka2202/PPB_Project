#include "scenarioengine.h"
#include "ppbcontrollerlib.h"
#include <QFile>
#include <QTextStream>
#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QThread>
#include <QFileInfo>

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
    // Lua API uses duty cycle in percent, just like the current FU widget.
    // Convert it to the three-byte zero-pulse duration expected by PPBController.
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

    lua.set_function("analyzePackets", [this]() -> sol::table {
        auto res = m_controller->analyzeLastPackets();
        sol::table t = lua.create_table();
        t["ber"] = res["ber"].toDouble();
        t["lost"] = res["lostPackets"].toInt();
        t["bitErrors"] = res["bitErrors"].toInt();
        t["totalSent"] = res["totalSent"].toInt();
        t["totalReceived"] = res["totalReceived"].toInt();
        t["validPackets"] = res["validPackets"].toInt();
        return t;
    });

    lua.set_function("generatorAvailable", [this]() { return luaGeneratorAvailable(); });
    lua.set_function("setGeneratorFrequency", [this](int ch, double f) { return luaSetGeneratorFrequency(ch, f); });
    lua.set_function("setGeneratorAmplitude", [this](int ch, double val, const std::string& unit) { return luaSetGeneratorAmplitude(ch, val, unit); });
    lua.set_function("setGeneratorOutput", [this](int ch, bool en) { return luaSetGeneratorOutput(ch, en); });
    lua.set_function("setGeneratorWaveform", [this](int ch, const std::string& wave) { return luaSetGeneratorWaveform(ch, wave); });
    lua.set_function("setGeneratorDutyCycle", [this](int ch, double percent) { return luaSetGeneratorDutyCycle(ch, percent); });
    lua.set_function("getGeneratorIdentity", [this]() { return luaGetGeneratorIdentity(); });

    lua["engine"] = this;
}

ScenarioEngine::~ScenarioEngine()
{
    stop();
}

void ScenarioEngine::stop()
{
    // stop() intentionally contains no QObject/thread-affine work. It is safe
    // to call directly from the controller thread while Lua is running.
    m_stopRequested.store(true, std::memory_order_release);
}

void ScenarioEngine::luaLog(const std::string &msg) {
    QString qmsg = QString::fromStdString(msg);
    LOG_INFO(LogCategory::GENERAL, "[SCENARIO] " + qmsg);
    emit logMessage(qmsg);
}

bool ScenarioEngine::loadScriptContent(const QString& content, const QString& scriptName)
{
    m_scriptName = scriptName;

    QString source = content;
    if (source.startsWith(QChar(0xFEFF))) {
        source = source.mid(1);
    }

    try {
        lua.script(source.toStdString());
        sol::function main = lua["main"];
        if (!main.valid()) {
            emit errorOccurred("Script does not contain 'main' function: " + m_scriptName);
            return false;
        }
    } catch (const sol::error &e) {
        emit errorOccurred(QString("Lua error in %1: %2").arg(m_scriptName, e.what()));
        return false;
    }

    return true;
}

bool ScenarioEngine::loadEmbeddedScript(const QString& name) {
    const QString path = QString(":/scenario/scripts/%1.lua").arg(name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open embedded script: " + path);
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    return loadScriptContent(content, name + ".lua");
}

bool ScenarioEngine::loadScript(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Cannot open file: " + fileName);
        return false;
    }

    QTextStream in(&file);
    const QString content = in.readAll();
    file.close();

    return loadScriptContent(content, QFileInfo(fileName).fileName());
}

bool ScenarioEngine::execute()
{
    LOG_OP_OPERATION(QString("========== Запуск скрипта: %1 ==========").arg(m_scriptName));
    m_stopRequested.store(false, std::memory_order_release);
    try {
        sol::function main = lua["main"];
        if (!main.valid()) {
            LOG_OP_OPERATION("Скрипт не содержит функцию main()");
            emit errorOccurred("Script does not contain 'main' function");
            return false;
        }
        main();

        if (m_stopRequested.load(std::memory_order_acquire)) {
            LOG_OP_OPERATION(QString("========== Скрипт %1 остановлен ==========").arg(m_scriptName));
            emit finished(false);
            return false;
        }

        LOG_OP_OPERATION(QString("========== Скрипт %1 завершён успешно ==========").arg(m_scriptName));
        emit finished(true);
    } catch (const sol::error &e) {
        LOG_OP_OPERATION(QString("========== Скрипт %1 завершён с ошибкой: %2 ==========").arg(m_scriptName, e.what()));
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
    if (m_stopRequested.load(std::memory_order_acquire)) return false;

    QEventLoop loop;
    bool success = false;
    bool timeout = false;

    QMetaObject::Connection conn;
    QTimer timer;
    timer.setSingleShot(true);
    timer.callOnTimeout([&] { timeout = true; loop.quit(); });

    QTimer stopPoll;
    stopPoll.setInterval(20);
    stopPoll.callOnTimeout([&] {
        if (m_stopRequested.load(std::memory_order_acquire)) {
            loop.quit();
        }
    });

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
    stopPoll.start();
    loop.exec();
    stopPoll.stop();

    QMetaObject::invokeMethod(m_controller, [&] { disconnect(conn); },
                              Qt::BlockingQueuedConnection);

    return success && !timeout && !m_stopRequested.load(std::memory_order_acquire);
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
    if (duration < 1 || duration > 27000 || dutyCycle < 1 || dutyCycle > 99) {
        emit logMessage(QString("Invalid FU timing: duration=%1 us, dutyCycle=%2%%")
                            .arg(duration).arg(dutyCycle));
        return false;
    }

    // duty = one / (one + zero) * 100
    // => zero = one * (100 / duty - 1)
    double zeroDuration = (static_cast<double>(duration) * 100.0 / dutyCycle) - duration;
    if (zeroDuration < 0.0 || zeroDuration >= 65536.0) {
        emit logMessage(QString("Calculated FU zero duration is out of range: %1 us").arg(zeroDuration));
        return false;
    }

    int whole = static_cast<int>(zeroDuration);
    int hundredths = static_cast<int>((zeroDuration - whole) * 100.0 + 0.5);
    if (hundredths >= 100) {
        ++whole;
        hundredths = 0;
    }
    if (whole >= 65536) {
        return false;
    }

    uint8_t encoded[3] = {
        static_cast<uint8_t>((whole >> 8) & 0xFF),
        static_cast<uint8_t>(whole & 0xFF),
        static_cast<uint8_t>(hundredths)
    };
    return luaSetFUReceive(address, duration, encoded);
}

bool ScenarioEngine::luaSetFUTransmit(uint16_t address, uint16_t duration, uint16_t dutyCycle)
{
    if (duration < 1 || duration > 27000 || dutyCycle < 1 || dutyCycle > 99) {
        emit logMessage(QString("Invalid FU timing: duration=%1 us, dutyCycle=%2%%")
                            .arg(duration).arg(dutyCycle));
        return false;
    }

    double zeroDuration = (static_cast<double>(duration) * 100.0 / dutyCycle) - duration;
    if (zeroDuration < 0.0 || zeroDuration >= 65536.0) {
        emit logMessage(QString("Calculated FU zero duration is out of range: %1 us").arg(zeroDuration));
        return false;
    }

    int whole = static_cast<int>(zeroDuration);
    int hundredths = static_cast<int>((zeroDuration - whole) * 100.0 + 0.5);
    if (hundredths >= 100) {
        ++whole;
        hundredths = 0;
    }
    if (whole >= 65536) {
        return false;
    }

    uint8_t encoded[3] = {
        static_cast<uint8_t>((whole >> 8) & 0xFF),
        static_cast<uint8_t>(whole & 0xFF),
        static_cast<uint8_t>(hundredths)
    };
    return luaSetFUTransmit(address, duration, encoded);
}

bool ScenarioEngine::luaSetFUReceive(uint16_t address, uint16_t duration, uint8_t dutyCycle[3])
{
    return waitForFUCommand(address, 0,
                            [this, address, duration, dutyCycle] {
                                m_controller->setFUReceive(address, duration, dutyCycle);
                            }, 2000);
}

bool ScenarioEngine::luaSetFUTransmit(uint16_t address, uint16_t duration, uint8_t dutyCycle[3])
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
    if (ms <= 0 || m_stopRequested.load(std::memory_order_acquire)) return;

    constexpr int stopPollMs = 20;
    int remaining = ms;
    while (remaining > 0 && !m_stopRequested.load(std::memory_order_acquire)) {
        const int chunk = qMin(stopPollMs, remaining);
        QThread::msleep(static_cast<unsigned long>(chunk));
        remaining -= chunk;
    }
}

bool ScenarioEngine::luaGeneratorAvailable() {
    return m_controller->isGeneratorAvailable();
}

bool ScenarioEngine::luaSetGeneratorFrequency(int channel, double freqHz) {
    if (!m_controller->isGeneratorAvailable()) {
        emit logMessage("Generator not available");
        return false;
    }
    m_controller->setGeneratorFrequency(channel, freqHz);
    return true;
}

bool ScenarioEngine::luaSetGeneratorAmplitude(int channel, double value, const std::string& unit) {
    if (!m_controller->isGeneratorAvailable()) return false;
    m_controller->setGeneratorAmplitude(channel, value, QString::fromStdString(unit));
    return true;
}

bool ScenarioEngine::luaSetGeneratorOutput(int channel, bool enable) {
    if (!m_controller->isGeneratorAvailable()) return false;
    m_controller->setGeneratorOutput(channel, enable);
    return true;
}

bool ScenarioEngine::luaSetGeneratorWaveform(int channel, const std::string& wave) {
    if (!m_controller->isGeneratorAvailable()) return false;
    m_controller->setGeneratorWaveform(channel, QString::fromStdString(wave));
    return true;
}

bool ScenarioEngine::luaSetGeneratorDutyCycle(int channel, double percent) {
    if (!m_controller->isGeneratorAvailable()) return false;
    m_controller->setGeneratorDutyCycle(channel, percent);
    return true;
}

std::string ScenarioEngine::luaGetGeneratorIdentity() {
    if (!m_controller->isGeneratorAvailable()) return "";
    return m_controller->getGeneratorIdentity().toStdString();
}
