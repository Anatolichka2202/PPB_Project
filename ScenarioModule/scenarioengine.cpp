#include "scenarioengine.h"
#include "ppbcontrollerlib.h"
#include <QFile>
#include <QTextStream>
#include <QEventLoop>

// ... остальной код без изменений
ScenarioEngine::ScenarioEngine(PPBController* controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    // Функция логирования
    lua.set_function("log", [this](const std::string &msg) {
        emit logMessage(QString::fromStdString(msg));
    });

    // Синхронный запрос статуса
    lua.set_function("requestStatusSync", [this](uint16_t address) -> bool {
        QEventLoop loop;
        bool success = false;
        QMetaObject::Connection conn = connect(m_controller, &PPBController::commandCompleted,
                                               [&](bool s, const QString&, TechCommand cmd) {
                                                   if (cmd == TechCommand::TS) {
                                                       success = s;
                                                       loop.quit();
                                                   }
                                               });
        m_controller->requestStatus(address);
        loop.exec();
        disconnect(conn);
        return success;
    });

    // Аналогично можно добавить другие команды:
    // sendTCSync, requestVersionSync и т.д.

    lua["engine"] = this;
}

ScenarioEngine::~ScenarioEngine()
{
}

bool ScenarioEngine::loadScript(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Cannot open file: " + fileName);
        return false;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    try {
        lua.script(content.toStdString());
    } catch (const sol::error &e) {
        emit errorOccurred(QString("Lua error: %1").arg(e.what()));
        return false;
    }
    return true;
}

bool ScenarioEngine::execute()
{
    try {
        sol::function main = lua["main"];
        if (!main.valid()) {
            emit errorOccurred("Script does not contain 'main' function");
            return false;
        }
        main();
        emit finished(true);
    } catch (const sol::error &e) {
        emit errorOccurred(QString("Execution error: %1").arg(e.what()));
        emit finished(false);
        return false;
    }
    return true;
}
