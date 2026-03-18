#include "pult.h"
#include "ui_pult.h"
#include <QString>
#include "dependencies.h"
#include <QTimer>

pult::pult(uint16_t address, PPBController* controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::pult)
    , m_controller(controller)
    , m_address(address)
    , m_statusTimer(nullptr)
{
    ui->setupUi(this);
    setWindowTitle(QString("Пульт управления ППБ (адрес: %1)").arg(address));

    if (!m_controller) {
        LOG_UI_ALERT("Пульт: передан нулевой контроллер");
        return;
    }

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        ui->statusbar->clear();
    });

    // Подключаем сигналы контроллера
    connect(m_controller, &PPBController::errorOccurred,
            this, &pult::onControllerErrorOccurred);
    connect(m_controller, &PPBController::operationCompleted,
            this, &pult::onControllerOperationCompleted);
    connect(m_controller, &PPBController::analysisStarted,
            this, &pult::onAnalysisStarted);
    connect(m_controller, &PPBController::analysisProgress,
            this, &pult::onAnalysisProgress);
    connect(m_controller, &PPBController::analysisComplete,
            this, &pult::onAnalysisComplete);
    connect(m_controller, &PPBController::fullStateUpdated,
            this, &pult::onFullStateUpdated);
    // Новый сигнал для настроек
    connect(m_controller, &PPBController::userSettingsChanged,
            this, &pult::onUserSettingsChanged);

    // Подключаем сигналы от полей ввода
    connect(ui->power_set_up_ch1, &QLineEdit::textChanged,
            this, &pult::onPower1Changed);
    connect(ui->power_set_up_ch2, &QLineEdit::textChanged,
            this, &pult::onPower2Changed);
    connect(ui->disactiveFu, &QCheckBox::toggled,
            this, &pult::onFuBlockedToggled);
    connect(ui->isReboot, &QCheckBox::toggled,
            this, &pult::onRebootToggled);
    connect(ui->isreseterror, &QCheckBox::toggled,
            this, &pult::onResetErrorsToggled);

    // Инициализируем поля из настроек
    refreshSettings();

    LOG_UI_OPERATION("Пульт инициализирован для адреса " + QString::number(address));
}

pult::~pult()
{
    delete ui;
}

int pult::getPpbIndex() const
{
    for (int i = 0; i < 16; ++i) {
        if (m_address == (1 << i))
            return i;
    }
    return -1;
}

void pult::refreshSettings()
{
    if (!m_controller) return;
    int index = getPpbIndex();
    if (index >= 0) {
        onUserSettingsChanged(index);
    }
}

// ==================== КОМАНДЫ ====================

void pult::on_TSComand_clicked()
{
    if (m_controller)
        m_controller->requestStatus(m_address);
}

void pult::on_TCCommand_clicked()
{
    if (m_controller)
        m_controller->sendTC(m_address);
}

void pult::on_PRBS_S2MCommand_clicked()
{
    if (m_controller)
        m_controller->startPRBS_S2M(m_address);
}

void pult::on_PRBS_M2SCommand_clicked()
{
    if (m_controller)
        m_controller->startPRBS_M2S(m_address);
}

void pult::on_VERSComand_clicked()
{
    if (m_controller)
        m_controller->requestVersion(m_address);
}

void pult::on_VolumeComand_clicked()
{
    if (m_controller)
        m_controller->requestVolume(m_address);
}

void pult::on_ChecksumCommand_clicked()
{
    if (m_controller)
        m_controller->requestChecksum(m_address);
}

void pult::on_ProgramCommand_clicked()
{
    if (m_controller)
        m_controller->sendProgram(m_address);
}

void pult::on_CleanCommand_clicked()
{
    if (m_controller)
        m_controller->sendClean(m_address);
}

void pult::on_DropCommand_clicked()
{
    if (m_controller)
        m_controller->requestDroppedPackets(m_address);
}

void pult::on_BER_TCommand_clicked()
{
    if (m_controller)
        m_controller->requestBER_T(m_address);
}

void pult::on_BER_FCommand_clicked()
{
    if (m_controller)
        m_controller->requestBER_F(m_address);
}

void pult::on_AnalizeBttn_clicked()
{
    if (m_controller)
        m_controller->analize();
}

void pult::on_FabricNumber_clicked()
{
    if (m_controller)
        m_controller->requestFabricNumber(m_address);
}

// ==================== РЕДАКТИРОВАНИЕ ПОЛЕЙ ====================

void pult::onPower1Changed(const QString& text)
{
    bool ok;
    QString normalized = QString(text).replace(',', '.');
    float watts = normalized.toFloat(&ok);
    if (ok && m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setPowerSetting(index, 1, watts);
    }
}

void pult::onPower2Changed(const QString& text)
{
    bool ok;
    QString normalized = QString(text).replace(',', '.');
    float watts = normalized.toFloat(&ok);
    if (ok && m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setPowerSetting(index, 2, watts);
    }
}

void pult::onFuBlockedToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setFuBlockedSetting(index, checked);
    }
}

void pult::onRebootToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setRebootRequestedSetting(index, checked);
    }
}

void pult::onResetErrorsToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setResetErrorsSetting(index, checked);
    }
}

// ==================== ОБРАТНАЯ СВЯЗЬ ====================

void pult::onControllerErrorOccurred(const QString& error)
{
    QMessageBox::warning(this, "Ошибка", error);
    LOG_UI_ALERT("Pult error: " + error);
    ui->statusbar->setText("Ошибка: " + error);
    ui->statusbar->setStyleSheet("color: red; font-weight: bold;");
    if (m_statusTimer)
        m_statusTimer->start(5000);
}

void pult::onControllerOperationCompleted(bool success, const QString& message)
{
    if (success) {
        ui->statusbar->setText("✓ " + message);
        ui->statusbar->setStyleSheet("color: green; font-weight: bold;");
        LOG_UI_RESULT("Pult operation: " + message);
    } else {
        ui->statusbar->setText("✗ " + message);
        ui->statusbar->setStyleSheet("color: orange; font-weight: bold;");
        LOG_UI_ALERT("Pult operation failed: " + message);
    }
    if (m_statusTimer)
        m_statusTimer->start(3000);
}

void pult::onAnalysisStarted()
{
    ui->statusbar->setText("📊 Анализ начат...");
    ui->statusbar->setStyleSheet("color: blue; font-weight: bold;");
}

void pult::onAnalysisProgress(int percent)
{
    ui->statusbar->setText(QString("📊 Анализ: %1%").arg(percent));
}

void pult::onAnalysisComplete(const QString& summary, const QVariantMap& details)
{
    ui->statusbar->setText("✅ Анализ завершен");
    ui->statusbar->setStyleSheet("color: green; font-weight: bold;");
    QMessageBox::information(this, "Результаты анализа", summary);
    if (m_statusTimer)
        m_statusTimer->start(5000);
}

// ==================== ОБНОВЛЕНИЕ ОТ КОНТРОЛЛЕРА ====================

void pult::onFullStateUpdated(uint8_t ppbIndex)
{
    if (ppbIndex != getPpbIndex()) return;
    // Здесь можно обновить индикаторы, если они есть
    // Поля ввода не трогаем – они обновляются через onUserSettingsChanged
}

void pult::onUserSettingsChanged(uint8_t ppbIndex)
{
    if (ppbIndex != getPpbIndex() || !m_controller) return;

    auto settings = m_controller->getUserSettings(ppbIndex);

    ui->power_set_up_ch1->blockSignals(true);
    ui->power_set_up_ch1->setText(QString::number(settings.power1));
    ui->power_set_up_ch1->blockSignals(false);

    ui->power_set_up_ch2->blockSignals(true);
    ui->power_set_up_ch2->setText(QString::number(settings.power2));
    ui->power_set_up_ch2->blockSignals(false);

    ui->disactiveFu->blockSignals(true);
    ui->disactiveFu->setChecked(settings.fuBlocked);
    ui->disactiveFu->blockSignals(false);

    ui->isReboot->blockSignals(true);
    ui->isReboot->setChecked(settings.rebootRequested);
    ui->isReboot->blockSignals(false);

    ui->isreseterror->blockSignals(true);
    ui->isreseterror->setChecked(settings.resetErrors);
    ui->isreseterror->blockSignals(false);
}
