#include "testerwindow.h"
#include "ui_testerwindow.h"

#include "ppbcontrollerlib.h"
#include "pult.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QCloseEvent>
#include <QThread>
#include <QHostAddress>
#include "dependencies.h"
#include "akipfacade.h"              // для dynamic_cast
#include "grattenga1483controller.h" // для dynamic_cast
#include "connectionwidget.h"
#include "controlwidget.h"
#include "akipwidget.h"
#include "fuwidget.h"
#include "dataviewwidget.h"
#include "scenariowidget.h"
#include "logwidget.h"
#include "performancewindow.h"
#include "applicationmanager.h"
#include <QSettings>
#include <QApplication>
#include <QDebug>

TesterWindow::TesterWindow(PPBController* controller, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TesterWindow)
    , m_controller(controller)
{
    ui->setupUi(this);

    ui->mainSplitter->setStretchFactor(0, 3);
    ui->mainSplitter->setStretchFactor(1, 7);

    // Скрываем индикатор выбранных ППБ по умолчанию
    ui->selectedCountLabel->setVisible(false);

    // Создаём вкладки ППБ
    setupPpbTabs(16);

    // Подключение сигналов контроллера
    connect(m_controller, &PPBController::fullStateUpdated,
            this, &TesterWindow::onFullStateUpdated);
    connect(m_controller, &PPBController::connectionStateChanged,
            this, &TesterWindow::onConnectionStateChanged);
    connect(m_controller, &PPBController::errorOccurred,
            this, &TesterWindow::onErrorOccurred);
    connect(m_controller, &PPBController::busyChanged,
            this, &TesterWindow::onBusyChanged);
    connect(m_controller, &PPBController::operationProgress,
            this, &TesterWindow::onOperationProgress);
    connect(m_controller, &PPBController::operationCompleted,
            this, &TesterWindow::onOperationCompleted);

    // Подключение сигналов от виджетов левой панели
    connect(ui->connectionWidget, &ConnectionWidget::connectRequested,
            this, &TesterWindow::onConnectRequested);
    connect(ui->connectionWidget, &ConnectionWidget::disconnectRequested,
            this, &TesterWindow::onDisconnectRequested);
    connect(ui->connectionWidget, &ConnectionWidget::exitClicked,
            this, &TesterWindow::onExitClicked);
    connect(ui->pultBtn, &QPushButton::clicked,
            this, &TesterWindow::onPultClicked);
    connect(ui->metricsBtn, &QPushButton::clicked,
            this, &TesterWindow::onMetricsClicked);
    connect(ui->akipPultBtn, &QPushButton::clicked,
            this, &TesterWindow::onAkipPultClicked);
    connect(ui->reconnectGeneratorBtn, &QPushButton::clicked,
            this, &TesterWindow::onReconnectGeneratorClicked);

    // DataViewWidget
    connect(ui->dataViewWidget, &DataViewWidget::displayModeChanged,
            this, &TesterWindow::onDisplayModeChanged);

    // ControlWidget (проброс через окно для множественного выбора)
    connect(ui->controlWidget, &ControlWidget::pollStatusClicked,
            this, [this]() {
                if (m_controller->isBusy()) {
                    statusBar()->showMessage("Контроллер занят", 2000);
                    return;
                }
                auto addrs = getSelectedAddresses();
                for (uint16_t addr : addrs)
                    m_controller->requestStatus(addr);
            });
    connect(ui->controlWidget, &ControlWidget::resetClicked,
            this, [this]() {
                auto addrs = getSelectedAddresses();
                for (uint16_t addr : addrs)
                    m_controller->sendTC(addr);
            });
    connect(ui->controlWidget, &ControlWidget::testSequenceClicked,
            this, [this]() {
                auto addrs = getSelectedAddresses();
                for (uint16_t addr : addrs)
                    m_controller->runFullTest(addr);
            });
    connect(ui->controlWidget, &ControlWidget::autoPollToggled,
            this, [this](bool checked) {
                if (checked)
                    m_controller->startAutoPoll(5000);
                else
                    m_controller->stopAutoPoll();
            });

    // FuWidget
    connect(ui->fuWidget, &FuWidget::sendFuCommand,
            this, [this](bool transmit, uint16_t duration, uint16_t dutyCycle) {
                auto addrs = getSelectedAddresses();
                for (uint16_t addr : addrs) {
                    if (transmit)
                        m_controller->setFUTransmit(addr);
                    else
                        m_controller->setFUReceive(addr, duration, dutyCycle);
                }
            });

    // ScenarioWidget (заглушки)
    connect(ui->scenarioWidget, &ScenarioWidget::loadScenario,
            this, [this]() {
                QString fileName = QFileDialog::getOpenFileName(this, "Загрузить сценарий",
                                                                "", "Сценарии (*.json *.xml)");
                if (!fileName.isEmpty())
                    ui->scenarioWidget->setScenarioFileName(QFileInfo(fileName).fileName());
            });
    connect(ui->scenarioWidget, &ScenarioWidget::runScenario,
            this, []() { /* TODO */ });
    connect(ui->scenarioWidget, &ScenarioWidget::stopScenario,
            this, []() { /* TODO */ });

    // Восстановление геометрии и состояния сплиттера
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    ui->rightSplitter->restoreState(settings.value("rightSplitter").toByteArray());

    // Инициализация UI генератора
    updateGeneratorUi();
}

TesterWindow::~TesterWindow()
{
    delete ui;
}

void TesterWindow::setupPpbTabs(int count)
{
    m_statusWidgets.clear();
    for (int i = 0; i < count; ++i) {
        QString name = QString("ППБ%1").arg(i + 1);
        ui->ppbTabBar->addTab(name);
        StatusWidget* sw = new StatusWidget(ui->ppbStack);
        ui->ppbStack->addWidget(sw);
        m_statusWidgets.append(sw);
    }
    m_selectedTabs.insert(0);
    ui->ppbTabBar->setCurrentIndex(0);
    ui->ppbStack->setCurrentIndex(0);
    updateSelectedCountLabel();

    // Подключение сигналов вкладок
    connect(ui->ppbTabBar, &QTabBar::tabBarClicked,
            this, &TesterWindow::onTabClicked);
    connect(ui->ppbTabBar, &QTabBar::currentChanged,
            this, &TesterWindow::onTabBarCurrentChanged);
}

void TesterWindow::updateSelectedCountLabel()
{
    int count = m_selectedTabs.size();
    if (count > 1) {
        ui->selectedCountLabel->setText(QString("Выбрано: %1 ППБ").arg(count));
        ui->selectedCountLabel->setVisible(true);
    } else {
        ui->selectedCountLabel->setVisible(false);
    }
}

void TesterWindow::onTabClicked(int index)
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();

    if (mods & Qt::ControlModifier) {
        if (m_selectedTabs.contains(index))
            m_selectedTabs.remove(index);
        else
            m_selectedTabs.insert(index);
    }
    else if (mods & Qt::ShiftModifier && m_lastSelectedTab != -1) {
        int start = qMin(m_lastSelectedTab, index);
        int end = qMax(m_lastSelectedTab, index);
        for (int i = start; i <= end; ++i)
            m_selectedTabs.insert(i);
    }
    else {
        m_selectedTabs.clear();
        m_selectedTabs.insert(index);
    }

    m_lastSelectedTab = index;
    ui->ppbTabBar->setCurrentIndex(index);
    ui->ppbStack->setCurrentIndex(index);
    updateSelectedCountLabel();
}

void TesterWindow::onTabBarCurrentChanged(int index)
{
    ui->ppbStack->setCurrentIndex(index);
}

uint16_t TesterWindow::getSelectedAddress() const
{
    int index = ui->ppbTabBar->currentIndex();
    return (index >= 0 && index < 16) ? (1 << index) : 0;
}

QList<uint16_t> TesterWindow::getSelectedAddresses() const
{
    QList<uint16_t> addrs;
    for (int idx : m_selectedTabs) {
        if (idx >= 0 && idx < 16)
            addrs.append(1 << idx);
    }
    return addrs;
}

void TesterWindow::onFullStateUpdated(uint8_t ppbIndex)
{
    if (ppbIndex < m_statusWidgets.size()) {
        auto state = m_controller->getFullState(ppbIndex);
        m_statusWidgets[ppbIndex]->updateState(state, m_displayAsCodes);
    }
}

void TesterWindow::onDisplayModeChanged(bool codes)
{
    m_displayAsCodes = codes;
    int current = ui->ppbTabBar->currentIndex();
    if (current >= 0 && current < m_statusWidgets.size()) {
        auto state = m_controller->getFullState(current);
        m_statusWidgets[current]->updateState(state, codes);
    }
}

void TesterWindow::onConnectionStateChanged(PPBState state)
{
    ui->connectionWidget->setConnectionState(state, m_controller->isBusy());
}

void TesterWindow::onErrorOccurred(const QString &error)
{
    statusBar()->showMessage("Ошибка: " + error, 5000);
}

void TesterWindow::onBusyChanged(bool busy)
{
    ui->controlWidget->setBusy(busy);
}

void TesterWindow::onOperationProgress(int current, int total, const QString &operation)
{
    statusBar()->showMessage(QString("%1: %2/%3").arg(operation).arg(current).arg(total), 3000);
}

void TesterWindow::onOperationCompleted(bool success, const QString &message)
{
    statusBar()->showMessage(message, 5000);
    if (!success) {
        QMessageBox::warning(this, "Ошибка операции", message);
    }
}

// ---------- Слоты левой панели ----------
void TesterWindow::onConnectRequested(const QString &ip, quint16 port)
{
    // Новая логика: подключаемся к бриджу (ping), затем отправляем TS на выбранные ППБ
    // Пока оставляем как есть, но можно изменить после доработки контроллера
    uint16_t addr = getSelectedAddress();
    m_controller->connectToPPB(addr, ip, port);
}

void TesterWindow::onDisconnectRequested()
{
    m_controller->disconnect();
}

void TesterWindow::onExitClicked()
{
    close();
}


void TesterWindow::onPultClicked()
{
    if (m_selectedTabs.isEmpty()) return;
    int index = *m_selectedTabs.begin();
    uint16_t addr = (1 << index);
    // Создаём без родителя, чтобы окно было независимым
    pult* p = new pult(addr, m_controller, nullptr);
    p->setAttribute(Qt::WA_DeleteOnClose);
    p->setWindowFlags(Qt::Window); // можно и так, но без родителя уже окно
    p->show();
}
void TesterWindow::onMetricsClicked()
{
    static PerformanceWindow *perfWindow = nullptr;
    if (!perfWindow) {
        perfWindow = new PerformanceWindow(nullptr);
        perfWindow->setAttribute(Qt::WA_DeleteOnClose);
        perfWindow->setWindowFlags(Qt::Window); // можно и так, но без родителя уже окно

    }
    perfWindow->show();
    perfWindow->raise();
    perfWindow->activateWindow();
}

void TesterWindow::onAkipPultClicked()
{
    if (!m_signalGenerator) {
        QMessageBox::information(this, "АКИП пульт", "Генератор не подключён");
        return;
    }
    // TODO: реализовать окно akip_pult, принимающее IAkipController*
    // akip_pult* p = new akip_pult(m_signalGenerator, this);
    // p->show();
    QMessageBox::information(this, "АКИП пульт", "Функция в разработке");
}

void TesterWindow::onReconnectGeneratorClicked()
{
    statusBar()->showMessage("Переподключение генератора...", 2000);
    // Здесь можно вызвать метод ApplicationManager для повторного обнаружения
    // auto& manager = ApplicationManager::instance();
    // manager.reconnectGenerator(); // нужно добавить такой метод
}

// ---------- Управление генератором ----------
void TesterWindow::setSignalGeneratorController(IAkipController* ctrl)
{
    m_signalGenerator = ctrl;
    updateGeneratorUi();
}

void TesterWindow::updateGeneratorUi()
{
    if (m_signalGenerator && m_signalGenerator->isAvailable()) {
        // Определяем тип контроллера и показываем соответствующий виджет
        // Пока предполагаем только AkipWidget
        ui->generatorStack->setCurrentWidget(ui->akipWidget);
    } else {
        ui->generatorStack->setCurrentWidget(ui->manualGeneratorPage);
    }
}

// ---------- Закрытие окна ----------
void TesterWindow::closeEvent(QCloseEvent *event)
{
    if (m_controller->isBusy()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Подтверждение",
            "Выполняется операция. Закрыть программу?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("rightSplitter", ui->rightSplitter->saveState());

    event->accept();
}
