#ifndef PERFORMANCEWINDOW_H
#define PERFORMANCEWINDOW_H

#include <QWidget>
#include <QTabWidget>
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PerformanceWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PerformanceWindow(QWidget *parent = nullptr);
    ~PerformanceWindow();

private:
    void setupUi();

    QTabWidget *m_tabWidget;

    // Вкладка CPU (временно заглушка)
    QWidget *m_cpuTab;
    // QChartView *m_cpuChartView; // убрали

    // Вкладка очередей
    QWidget *m_queueTab;
    QTableView *m_queueTable;

    // Вкладка времени выполнения команд
    QWidget *m_commandTab;
    QTableView *m_commandTable;

    // Вкладка лога производительности
    QWidget *m_logTab;
};

#endif // PERFORMANCEWINDOW_H
