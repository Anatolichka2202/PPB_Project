#ifndef SCENARIOWIDGET_H
#define SCENARIOWIDGET_H

#include <QWidget>

namespace Ui {
class ScenarioWidget;
}

class ScenarioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScenarioWidget(QWidget *parent = nullptr);
    ~ScenarioWidget();

    void setScenarioFileName(const QString& fileName);   // отобразить имя файла

signals:
    void loadScenario();
    void runScenario();
    void stopScenario();

private slots:
    void onLoadClicked();
    void onRunClicked();
    void onStopClicked();

private:
    Ui::ScenarioWidget *ui;
};

#endif
