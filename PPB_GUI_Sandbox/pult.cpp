#include "pult.h"
#include "ui_pult.h"

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

    // Таймер для очистки статусной строки
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
    // **ВАЖНО**: подключаем новый сигнал fullStateUpdated
    connect(m_controller, &PPBController::fullStateUpdated,
            this, &pult::onFullStateUpdated);

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

    // Инициализируем поля текущим состоянием (если оно уже есть)
    int index = getPpbIndex();
    if (index >= 0) {
        auto state = m_controller->getFullState(index);
        // Заполняем поля, блокируя сигналы
        ui->power_set_up_ch1->blockSignals(true);
        ui->power_set_up_ch1->setText(QString::number(state.ch1.power));
        ui->power_set_up_ch1->blockSignals(false);

        ui->power_set_up_ch2->blockSignals(true);
        ui->power_set_up_ch2->setText(QString::number(state.ch2.power));
        ui->power_set_up_ch2->blockSignals(false);

        ui->disactiveFu->blockSignals(true);
        ui->disactiveFu->setChecked(state.fuBlocked);
        ui->disactiveFu->blockSignals(false);

        ui->isReboot->blockSignals(true);
        ui->isReboot->setChecked(state.rebootRequested);
        ui->isReboot->blockSignals(false);

        ui->isreseterror->blockSignals(true);
        ui->isreseterror->setChecked(state.resetErrors);
        ui->isreseterror->blockSignals(false);
    }

    LOG_UI_OPERATION("Пульт инициализирован для адреса " + QString::number(address));
}

pult::~pult()
{
    delete ui;
}

int pult::getPpbIndex() const
{
    // Предполагаем, что адрес — степень двойки (1 << index)
    for (int i = 0; i < 16; ++i) {
        if (m_address == (1 << i))
            return i;
    }
    return -1;
}

// ==================== КОМАНДЫ ====================

void pult::on_TSComand_clicked()
{
    if (m_controller) {
        m_controller->requestStatus(m_address);
    }
}

void pult::on_TCCommand_clicked()
{
    if (m_controller) {
        m_controller->sendTC(m_address);
    }
}

void pult::onPower1Changed(const QString& text)
{
    bool ok;
    float watts = text.toFloat(&ok);
    if (ok && m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setChannelPower(index, 1, watts);
    }
}

void pult::onPower2Changed(const QString& text)
{
    bool ok;
    float watts = text.toFloat(&ok);
    if (ok && m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setChannelPower(index, 2, watts);
    }
}

void pult::onFuBlockedToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setFuBlocked(index, checked);
    }
}

void pult::onRebootToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setRebootRequested(index, checked);
    }
}

void pult::onResetErrorsToggled(bool checked)
{
    if (m_controller) {
        int index = getPpbIndex();
        if (index >= 0)
            m_controller->setResetErrors(index, checked);
    }
}

void pult::onFullStateUpdated(uint8_t ppbIndex)
{
    if (ppbIndex != getPpbIndex()) return;

    auto state = m_controller->getFullState(ppbIndex);

    // Обновляем поля, блокируя сигналы, чтобы не вызвать рекурсию
    ui->power_set_up_ch1->blockSignals(true);
    ui->power_set_up_ch1->setText(QString::number(state.ch1.power));
    ui->power_set_up_ch1->blockSignals(false);

    ui->power_set_up_ch2->blockSignals(true);
    ui->power_set_up_ch2->setText(QString::number(state.ch2.power));
    ui->power_set_up_ch2->blockSignals(false);

    ui->disactiveFu->blockSignals(true);
    ui->disactiveFu->setChecked(state.fuBlocked);
    ui->disactiveFu->blockSignals(false);

    ui->isReboot->blockSignals(true);
    ui->isReboot->setChecked(state.rebootRequested);
    ui->isReboot->blockSignals(false);

    ui->isreseterror->blockSignals(true);
    ui->isreseterror->setChecked(state.resetErrors);
    ui->isreseterror->blockSignals(false);
}

void pult::on_PRBS_S2MCommand_clicked()
{
    if (m_controller) {
        m_controller->startPRBS_S2M(m_address);
    }
}

void pult::on_PRBS_M2SCommand_clicked()
{
    if (m_controller) {
        m_controller->startPRBS_M2S(m_address);
    }
}

void pult::on_VERSComand_clicked()
{
    if (m_controller) {
        m_controller->requestVersion(m_address);
    }
}

void pult::on_VolumeComand_clicked()
{
    if (m_controller) {
        m_controller->requestVolume(m_address);
    }
}

void pult::on_ChecksumCommand_clicked()
{
    if (m_controller) {
        m_controller->requestChecksum(m_address);
    }
}

void pult::on_ProgramCommand_clicked()
{
    if (m_controller) {
        m_controller->sendProgram(m_address);
    }
}

void pult::on_CleanCommand_clicked()
{
    if (m_controller) {
        m_controller->sendClean(m_address);
    }
}

void pult::on_DropCommand_clicked()
{
    if (m_controller) {
        m_controller->requestDroppedPackets(m_address);
    }
}

void pult::on_BER_TCommand_clicked()
{
    if (m_controller) {
        m_controller->requestBER_T(m_address);
    }
}

void pult::on_BER_FCommand_clicked()
{
    if (m_controller) {
        m_controller->requestBER_F(m_address);
    }
}


void pult::onControllerErrorOccurred(const QString& error)
{
    // Показываем ошибку пользователю
    QMessageBox::warning(this, "Ошибка", error);
    LOG_UI_ALERT("Pult error: " + error);

    // Обновляем статусную метку
    ui->statusbar->setText("Ошибка: " + error);
    ui->statusbar->setStyleSheet("color: red; font-weight: bold;");

    // Очищаем статус через 5 секунд
    if (m_statusTimer) {
        m_statusTimer->start(5000);
    }
}

void pult::onControllerOperationCompleted(bool success, const QString& message)
{
    // Обновляем статус в UI
    if (success) {
        ui->statusbar->setText("✓ " + message);
        ui->statusbar->setStyleSheet("color: green; font-weight: bold;");
        LOG_UI_RESULT("Pult operation: " + message);
    } else {
        ui->statusbar->setText("✗ " + message);
        ui->statusbar->setStyleSheet("color: orange; font-weight: bold;");
       LOG_UI_ALERT("Pult operation failed: " + message);
    }

    // Очищаем статус через 3 секунды
    if (m_statusTimer) {
        m_statusTimer->start(3000);
    }
}



void pult::on_AnalizeBttn_clicked()
{
    if (m_controller) {
        m_controller->analize();
    }
}

void pult::onAnalysisStarted() {
    ui->statusbar->setText("📊 Анализ начат...");
    ui->statusbar->setStyleSheet("color: blue; font-weight: bold;");
}

void pult::onAnalysisProgress(int percent) {
    ui->statusbar->setText(QString("📊 Анализ: %1%").arg(percent));
}

void pult::onAnalysisComplete(const QString& summary, const QVariantMap& details) {
    ui->statusbar->setText("✅ Анализ завершен");
    ui->statusbar->setStyleSheet("color: green; font-weight: bold;");

    // Можно показать диалог с результатами
    QMessageBox::information(this, "Результаты анализа", summary);

    // Очищаем статус через 5 секунд
    if (m_statusTimer) {
        m_statusTimer->start(5000);
    }
}

void pult::on_FabricNumber_clicked()
{
    if (m_controller) {
        m_controller->requestFabricNumber(m_address);
    }
}

