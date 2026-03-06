#include "performancewindow.h"
#include <QLabel>

PerformanceWindow::PerformanceWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

PerformanceWindow::~PerformanceWindow()
{
}

void PerformanceWindow::setupUi()
{
    setWindowTitle("Мониторинг производительности");
    resize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    // Вкладка CPU (заглушка)
    m_cpuTab = new QWidget;
    QVBoxLayout *cpuLayout = new QVBoxLayout(m_cpuTab);
    QLabel *cpuPlaceholder = new QLabel("График загрузки CPU (будет добавлен позже)");
    cpuPlaceholder->setAlignment(Qt::AlignCenter);
    cpuLayout->addWidget(cpuPlaceholder);
    m_tabWidget->addTab(m_cpuTab, "Загрузка CPU");

    // Вкладка очередей
    m_queueTab = new QWidget;
    QVBoxLayout *queueLayout = new QVBoxLayout(m_queueTab);
    m_queueTable = new QTableView;
    queueLayout->addWidget(m_queueTable);
    m_tabWidget->addTab(m_queueTab, "Очереди команд");

    // Вкладка времени выполнения
    m_commandTab = new QWidget;
    QVBoxLayout *cmdLayout = new QVBoxLayout(m_commandTab);
    m_commandTable = new QTableView;
    cmdLayout->addWidget(m_commandTable);
    m_tabWidget->addTab(m_commandTab, "Время выполнения");

    // Вкладка лога
    m_logTab = new QWidget;
    QVBoxLayout *logLayout = new QVBoxLayout(m_logTab);
    QLabel *logPlaceholder = new QLabel("Лог производительности (будет добавлен позже)");
    logPlaceholder->setAlignment(Qt::AlignCenter);
    logLayout->addWidget(logPlaceholder);
    m_tabWidget->addTab(m_logTab, "Лог производительности");

    mainLayout->addWidget(m_tabWidget);

    QPushButton *closeBtn = new QPushButton("Закрыть");
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    mainLayout->addWidget(closeBtn);
}
