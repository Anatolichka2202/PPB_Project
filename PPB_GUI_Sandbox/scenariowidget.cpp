#include "scenariowidget.h"
#include "ui_scenariowidget.h"

ScenarioWidget::ScenarioWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ScenarioWidget)
{
    ui->setupUi(this);

    connect(ui->pushButtonLoadScenario, &QPushButton::clicked,
            this, &ScenarioWidget::onLoadClicked);
    connect(ui->pushButtonRunScenario, &QPushButton::clicked,
            this, &ScenarioWidget::onRunClicked);
    connect(ui->pushButtonStopScenario, &QPushButton::clicked,
            this, &ScenarioWidget::onStopClicked);
    qDebug() << "Constructor of ConnectionWidget";
}

ScenarioWidget::~ScenarioWidget()
{
    delete ui;
}

void ScenarioWidget::setScenarioFileName(const QString& fileName)
{
    ui->labelScenarioFile->setText(fileName);
}

void ScenarioWidget::onLoadClicked()   { emit loadScenario(); }
void ScenarioWidget::onRunClicked()    { emit runScenario(); }
void ScenarioWidget::onStopClicked()   { emit stopScenario(); }
