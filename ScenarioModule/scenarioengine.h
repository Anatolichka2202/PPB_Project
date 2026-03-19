// scenarioengine.h (изменения)
#ifndef SCENARIOENGINE_H
#define SCENARIOENGINE_H

#include <QObject>
#include <sol/sol.hpp>

class PPBController;  // <-- добавить эту строку вместо #include

class ScenarioEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioEngine(PPBController* controller, QObject *parent = nullptr);
    ~ScenarioEngine();

    bool loadScript(const QString &fileName);
    bool execute();

signals:
    void logMessage(const QString &msg);
    void errorOccurred(const QString &err);
    void finished(bool success);

private:
    sol::state lua;
    PPBController* m_controller;
};

#endif // SCENARIOENGINE_H
