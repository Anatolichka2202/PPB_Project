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

TesterWindow::TesterWindow(PPBController* controller, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TesterWindow)
    , m_controller(controller)
    , m_pultWindow(nullptr)
    , m_displayAsCodes(false)
    , m_currentPPBIndex(0)
    , m_isExiting(false)
{
    qDebug() << "TesterWindow constructor start, thread:" << QThread::currentThread();
    qDebug() << "Controller address:" << m_controller;

    qDebug() << "Step 1: constructor start";
    ui->setupUi(this);
     qDebug() << "Step 2: ui setup done";
    ui->tabWidget->clear();

    const int ppbCount = 16;
    m_statusWidgets.reserve(ppbCount);

    qDebug() << "Step 3: tabWidget cleared";

    if (ui->controlWidget) {
        ui->controlWidget->setController(m_controller);
    }

    for (int i = 0; i < ppbCount; ++i) {
        QWidget *tab = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(tab);
        layout->setContentsMargins(0, 0, 0, 0);

        StatusWidget *status = new StatusWidget(tab);
        layout->addWidget(status);

        ui->tabWidget->addTab(tab, QString("ППБ%1").arg(i + 1));
        m_statusWidgets.append(status);
    }

    m_stackedAkip = findChild<QStackedWidget*>("stackedAkip");
    if (!m_stackedAkip) {
        qWarning() << "stackedAkip not found in UI!";
    }

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, &TesterWindow::onTabChanged);
     qDebug() << "Step 5: all widgets created";
    QTimer::singleShot(0, this, [this]() {
        if (!m_controller) {
            //LOG_UI_ALERT("TesterWindow: контроллер не передан!");
            QMessageBox::critical(this, "Ошибка",
                                  "Контроллер не инициализирован. Приложение будет закрыто.");
            return;
        }

        initializeUI();
        connectSignals();
        updateConnectionUI();
        updateControlsState();

        //LOG_UI_STATUS("Программа управления ППБ запущена");
        //LOG_TECH_DEBUG("TesterWindow delayed init finished");
    });
    qDebug() << "Step 6: constructor finished";
}

TesterWindow::~TesterWindow()
{
    m_isExiting = true;
    if (m_controller && m_controller->parent() != this) {
        m_controller = nullptr;
    }
    delete ui;
}

void TesterWindow::initializeUI()
{
    onDisplayModeChanged(false);
}

void TesterWindow::connectSignals()
{
    connect(ui->connectionWidget, &ConnectionWidget::connectRequested,
            this, [this](const QString& ip, quint16 port) {
                uint16_t addr = getSelectedAddress();
                m_controller->connectToPPB(addr, ip, port);
            });
    connect(ui->connectionWidget, &ConnectionWidget::disconnectRequested,
            this, [this]() { m_controller->disconnect(); });
    connect(ui->connectionWidget, &ConnectionWidget::exitClicked,
            this, &TesterWindow::onExitClicked);

    connect(ui->controlWidget, &ControlWidget::pollStatusClicked,
            this, [this]() {
                if (m_controller->isBusy()) {
                    showStatusMessage("Контроллер занят, повторите позже", 2000);
                    return;
                }
                uint16_t addr = getSelectedAddress();
                m_controller->requestStatus(addr);
            });
    connect(ui->controlWidget, &ControlWidget::resetClicked,
            this, [this]() {
                uint16_t addr = getSelectedAddress();
                m_controller->sendTC(addr);
            });
    connect(ui->controlWidget, &ControlWidget::testSequenceClicked,
            this, &TesterWindow::onTestSequenceClicked);
    connect(ui->controlWidget, &ControlWidget::autoPollToggled,
            this, &TesterWindow::onAutoPollToggled);
    connect(ui->controlWidget, &ControlWidget::ppbSelected,
            this, &TesterWindow::onPPBSelected);

    connect(ui->akipWidget, &AkipWidget::applyClicked,
            this, &TesterWindow::onApplyParametersClicked);

    connect(ui->fuWidget, &FuWidget::sendFuCommand,
            this, [this](bool transmit, uint16_t duration, uint16_t dutyCycle) {
                uint16_t addr = getSelectedAddress();
                if (transmit) {
                    m_controller->setFUTransmit(addr);
                } else {
                    m_controller->setFUReceive(addr, duration, dutyCycle);
                }
            });

    connect(ui->dataViewWidget, &DataViewWidget::displayModeChanged,
            this, &TesterWindow::onDisplayModeChanged);

    connect(ui->scenarioWidget, &ScenarioWidget::loadScenario,
            this, &TesterWindow::onLoadScenario);
    connect(ui->scenarioWidget, &ScenarioWidget::runScenario,
            this, &TesterWindow::onRunScenario);
    connect(ui->scenarioWidget, &ScenarioWidget::stopScenario,
            this, &TesterWindow::onStopScenario);

    connect(m_controller, &PPBController::connectionStateChanged,
            this, &TesterWindow::onControllerConnectionStateChanged);
    connect(m_controller, &PPBController::statusReceived,
            this, &TesterWindow::onControllerStatusReceived);
    connect(m_controller, &PPBController::errorOccurred,
            this, &TesterWindow::onControllerErrorOccurred);
    connect(m_controller, &PPBController::channelStateUpdated,
            this, &TesterWindow::onControllerChannelStateUpdated);
    connect(m_controller, &PPBController::autoPollToggled,
            ui->controlWidget, &ControlWidget::setAutoPollChecked);
    connect(m_controller, &PPBController::operationProgress,
            this, &TesterWindow::onOperationProgress);
    connect(m_controller, &PPBController::operationCompleted,
            this, &TesterWindow::onOperationCompleted);
    connect(m_controller, &PPBController::busyChanged,
            this, &TesterWindow::onControllerBusyChanged);
    connect(ui->akipWidget, &AkipWidget::applyClicked, this, &TesterWindow::onAkipApplyClicked);
    connect(m_controller, &PPBController::akipAvailabilityChanged,
            this, &TesterWindow::onAkipAvailabilityChanged);

}

// ==================== СЛОТЫ ДЛЯ UI ====================

void TesterWindow::onPollStatusClicked()
{
    uint16_t address = getSelectedAddress();
    m_controller->requestStatus(address);
}


void TesterWindow::onApplyParametersClicked()
{
    if (!m_controller) return;

    // Выключаем выходы
    m_controller->setAkipOutput(1, false);
    m_controller->setAkipOutput(2, false);
    QThread::msleep(20);

    // Канал A
    double freqA_Hz = ui->akipWidget->frequencyA() * 1e6; // МГц -> Гц
    double powerA_dBm = ui->akipWidget->powerA();
    m_controller->setAkipFrequency(1, freqA_Hz);
    m_controller->setAkipAmplitude(1, powerA_dBm, "DBM");
    m_controller->setAkipWaveform(1, "SINE");

    // Канал B
    double freqB_Hz = ui->akipWidget->frequencyB() * 1e6;
    double amplB_Vpp = ui->akipWidget->amplitudeB();
    double dutyB = ui->akipWidget->dutyCycleB();
    m_controller->setAkipFrequency(2, freqB_Hz);
    m_controller->setAkipAmplitude(2, amplB_Vpp, "VPP");
    m_controller->setAkipWaveform(2, "SQUARE");
    m_controller->setAkipDutyCycle(2, dutyB);

    // Включаем выходы, если нужно
    if (ui->akipWidget->outputA())
        m_controller->setAkipOutput(1, true);
    if (ui->akipWidget->outputB())
        m_controller->setAkipOutput(2, true);

    LOG_UI_OPERATION("Параметры АКИП применены");
}

void TesterWindow::onAutoPollToggled(bool checked)
{
    if (checked) {
        m_controller->startAutoPoll(5000);
    } else {
        m_controller->stopAutoPoll();
    }
}

void TesterWindow::onTestSequenceClicked()
{
    if (m_controller->isBusy()) {
        LOG_UI_ALERT("Внимание, Уже выполняется другая операция");
        //QMessageBox::warning(this, "Внимание", "Уже выполняется другая операция");
        return;
    }

    setLeftPanelEnabled(false);
    uint16_t address = getSelectedAddress();
    m_controller->runFullTest(address);
}

void TesterWindow::onPRBSM2SClicked()
{
    uint16_t address = getSelectedAddress();
    m_controller->startPRBS_M2S(address);
}

void TesterWindow::onPRBSS2MClicked()
{
    uint16_t address = getSelectedAddress();
    m_controller->startPRBS_S2M(address);
}

// ==================== СЛОТЫ ОТ КОНТРОЛЛЕРА ====================

void TesterWindow::onControllerConnectionStateChanged(PPBState state)
{
    updateConnectionUI();
    updateControlsState();
}

void TesterWindow::onControllerStatusReceived(uint16_t address, const QVector<QByteArray>& data)
{
    LOG_UI_STATUS(QString("Контроллер: Статус ППБ %1 получен (%2 пакетов)")
                      .arg(address).arg(data.size()));
}

void TesterWindow::onControllerErrorOccurred(const QString& error)
{
    LOG_UI_ALERT(error);
    showStatusMessage(error, 5000);
    updateControlsState();
    updateConnectionUI();
    LOG_TECH_DEBUG("Window: Обработана ошибка: " + error);
}

void TesterWindow::onControllerChannelStateUpdated(uint8_t ppbIndex, int channel, const UIChannelState& state)
{
    if (ppbIndex >= static_cast<uint8_t>(m_statusWidgets.size())) {
        LOG_UI_ALERT(QString("onControllerChannelStateUpdated: ppbIndex %1 вне диапазона").arg(ppbIndex));
        return;
    }
    StatusWidget* widget = m_statusWidgets[ppbIndex];
    if (!widget) {
        LOG_UI_ALERT(QString("onControllerChannelStateUpdated: widget для ppbIndex %1 равен nullptr").arg(ppbIndex));
        return;
    }
    if (channel == 1) {
        widget->setChannel1State(state, m_displayAsCodes);
    } else if (channel == 2) {
        widget->setChannel2State(state, m_displayAsCodes);
    }
}

void TesterWindow::onOperationProgress(int current, int total, const QString& operation)
{
    ui->statusbar->showMessage(QString("%1: %2/%3").arg(operation).arg(current).arg(total), 3000);
}

void TesterWindow::onOperationCompleted(bool success, const QString& message)
{
    setLeftPanelEnabled(true);
    ui->statusbar->showMessage(message, 5000);

    if (!success) {
       // QMessageBox::warning(this, "Ошибка операции", message);
        LOG_UI_ALERT(message);
    } else {
        LOG_OP_OPERATION("Операция завершена: " + message);
        LOG_UI_STATUS("Операция завершена: " + message);
    }
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

void TesterWindow::showStatusMessage(const QString& message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void TesterWindow::updateWindowTitle()
{
    QString title = "Программа управления ППБ";
    if (m_controller->isBusy()) {
        title += " [Выполняется операция]";
    }
    setWindowTitle(title);
}

void TesterWindow::closeEvent(QCloseEvent *event)
{
    if (!m_isExiting) {
        if (m_controller->isBusy()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение",
                "Выполняется операция. Вы уверены, что хотите выйти?",
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No) {
                event->ignore();
                return;
            }
        }
        LOG_SYSTEM("Программа завершена");
    }
    event->accept();
}

uint16_t TesterWindow::getSelectedAddress() const
{
    int index = ui->controlWidget->currentPPBIndex();
    if (index >= 0 && index < 16)
        return (1 << index);
    return 0;
}

void TesterWindow::updateControlsState()
{
    PPBState state = m_controller->connectionState();
    bool busy = m_controller->isBusy();
    bool connected = (state == PPBState::Ready);

    ui->controlWidget->setBusy(busy);
    ui->controlWidget->setConnected(connected);

    ui->akipWidget->setEnabled(!busy && connected);
    ui->fuWidget->setEnabled(!busy && connected);
    ui->scenarioWidget->setEnabled(!busy && connected);
}

void TesterWindow::updateConnectionUI()
{
    PPBState state = m_controller->connectionState();
    bool busy = m_controller->isBusy();

    ui->connectionWidget->setConnectionState(state, busy);
    updateWindowTitle();
    LOG_TECH_DEBUG(QString("TesterWindow::updateConnectionUI: state=%1, busy=%2")
                       .arg(static_cast<int>(state)).arg(busy));
}

void TesterWindow::setLeftPanelEnabled(bool enabled)
{
    ui->controlWidget->setEnabled(enabled);
    ui->akipWidget->setEnabled(enabled);
    ui->fuWidget->setEnabled(enabled);
    ui->scenarioWidget->setEnabled(enabled);
}

// ==================== СЛОТЫ UI ====================

void TesterWindow::onDisplayModeChanged(bool showCodes)
{
    m_displayAsCodes = showCodes;
    int current = ui->tabWidget->currentIndex();
    if (current >= 0 && current < m_statusWidgets.size()) {
        UIChannelState ch1 = m_controller->getChannelState(current, 1);
        UIChannelState ch2 = m_controller->getChannelState(current, 2);
        m_statusWidgets[current]->setChannel1State(ch1, showCodes);
        m_statusWidgets[current]->setChannel2State(ch2, showCodes);
    }
}

void TesterWindow::onPPBSelected(int index)
{
    if (index < 0 || index >= m_statusWidgets.size())
        return;
    ui->tabWidget->setCurrentIndex(index);
    m_currentPPBIndex = static_cast<uint8_t>(index);
    UIChannelState ch1 = m_controller->getChannelState(index, 1);
    UIChannelState ch2 = m_controller->getChannelState(index, 2);
    m_statusWidgets[index]->setChannel1State(ch1, m_displayAsCodes);
    m_statusWidgets[index]->setChannel2State(ch2, m_displayAsCodes);
}

void TesterWindow::onExitClicked()
{
    close();
}

void TesterWindow::onControllerBusyChanged(bool busy)
{
    LOG_TECH_DEBUG("TesterWindow: состояние занятости изменилось: " +
                   QString(busy ? "занят" : "свободен"));
    updateControlsState();
    updateConnectionUI();
    updateWindowTitle();
}

void TesterWindow::onTabChanged(int index)
{
    if (index < 0 || index >= m_statusWidgets.size())
        return;
    ui->controlWidget->setCurrentPPBIndex(index);
    m_currentPPBIndex = static_cast<uint8_t>(index);
}

void TesterWindow::onLoadScenario()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Загрузить сценарий", "", "Сценарии (*.json *.xml)");
    if (!fileName.isEmpty()) {
        ui->scenarioWidget->setScenarioFileName(QFileInfo(fileName).fileName());
        // TODO: загрузить сценарий
    }
}

void TesterWindow::onRunScenario()
{
    // TODO: запустить сценарий
}

void TesterWindow::onStopScenario()
{
    // TODO: остановить сценарий
}

void TesterWindow::on_pult_active_clicked()
{
    if (m_pultWindow) {
        m_pultWindow->show();
        m_pultWindow->raise();
        m_pultWindow->activateWindow();
        return;
    }

    uint16_t address = getSelectedAddress();

    m_pultWindow = new pult(address, m_controller, nullptr);
    m_pultWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_pultWindow->setWindowTitle(QString("Пульт управления ППБ %1").arg(ui->controlWidget->currentPPBIndex()));

    m_pultWindow->setWindowIcon(QIcon("../remote-control.png"));

    connect(m_pultWindow, &pult::destroyed, this, [this]() {
        LOG_TECH_DEBUG("TesterWindow: пульт уничтожен");
        m_pultWindow = nullptr;
    });

    m_pultWindow->show();

    LOG_UI_OPERATION("CONTROLLER: Активирован пульт управления для ППБ " +
                     QString::number(address, 16));
}
void TesterWindow::onAkipApplyClicked()
{
    if (!m_controller) return;

    // Выключаем выходы
    m_controller->setAkipOutput(1, false);
    m_controller->setAkipOutput(2, false);
    QThread::msleep(20);

    // Канал A
    double freqA_Hz = ui->akipWidget->frequencyA() * 1e6;
    double powerA_dBm = ui->akipWidget->powerA();
    m_controller->setAkipFrequency(1, freqA_Hz);
    m_controller->setAkipAmplitude(1, powerA_dBm, "DBM");
    // Предполагаем, что форма сигнала для канала A всегда синус (можно добавить выбор позже)
    m_controller->setAkipWaveform(1, "SINE");

    // Канал B
    double freqB_Hz = ui->akipWidget->frequencyB() * 1e6;
    double amplB_Vpp = ui->akipWidget->amplitudeB();
    double dutyB = ui->akipWidget->dutyCycleB();
    m_controller->setAkipFrequency(2, freqB_Hz);
    m_controller->setAkipAmplitude(2, amplB_Vpp, "VPP");
    m_controller->setAkipWaveform(2, "SQUARE");
    m_controller->setAkipDutyCycle(2, dutyB);

    // Включаем выходы, если нужно
    if (ui->akipWidget->outputA())
        m_controller->setAkipOutput(1, true);
    if (ui->akipWidget->outputB())
        m_controller->setAkipOutput(2, true);

    LOG_UI_OPERATION("Параметры АКИП применены");
}
void TesterWindow::onAkipAvailabilityChanged(bool available)
{
    ui->akipWidget->setEnabled(available);
    if (!available) {
        ui->statusbar->showMessage("АКИП не доступен, ручной режим", 5000);
    }
}

void TesterWindow::setSignalGeneratorController(IAkipController* ctrl)
{
    m_signalGenerator = ctrl;
    if (!ctrl) {
        // Нет устройства – показываем пустую страницу и блокируем
        if (m_stackedAkip) m_stackedAkip->setCurrentIndex(1); // пустая страница
        ui->akipWidget->setEnabled(false);
        if (m_grattenWidget) m_grattenWidget->setEnabled(false);
        showStatusMessage("Генератор не доступен, ручной режим", 5000);
        return;
    }

    // Подключаем сигналы контроллера для обновления UI (опционально)
    connect(ctrl, &IAkipController::availabilityChanged,
            this, &TesterWindow::onGeneratorAvailabilityChanged, Qt::UniqueConnection);

    // Определяем тип контроллера
    if (dynamic_cast<AkipFacade*>(ctrl)) {
        // АКИП
        if (m_stackedAkip) m_stackedAkip->setCurrentWidget(ui->akipWidget);
        ui->akipWidget->setEnabled(true);
        if (m_grattenWidget) m_grattenWidget->setEnabled(false);
    }
    else if (dynamic_cast<GrattenGa1483Controller*>(ctrl)) {
        // Gratten – создаём виджет, если ещё не создан
        if (!m_grattenWidget) {
            m_grattenWidget = new GrattenControlWidget(ctrl, this);
            m_stackedAkip->addWidget(m_grattenWidget);
        }
        m_stackedAkip->setCurrentWidget(m_grattenWidget);
        m_grattenWidget->setEnabled(true);
        ui->akipWidget->setEnabled(false);
    }
}

void TesterWindow::onGeneratorAvailabilityChanged(bool available)
{
    if (!available) {
        if (m_stackedAkip) m_stackedAkip->setCurrentIndex(1);
        showStatusMessage("Связь с генератором потеряна", 5000);
    }
}

void TesterWindow::on_btnPerformance_clicked()
{
    static PerformanceWindow *perfWindow = nullptr;
    if (!perfWindow) {
        perfWindow = new PerformanceWindow(this);
        perfWindow->setAttribute(Qt::WA_DeleteOnClose);
    }
    perfWindow->show();
    perfWindow->raise();
    perfWindow->activateWindow();
}
