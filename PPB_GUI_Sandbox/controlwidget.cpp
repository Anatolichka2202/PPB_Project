#include "controlwidget.h"
#include "ui_controlwidget.h"
#include "dataconverter.h"
#include <QValidator>
ControlWidget::ControlWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ControlWidget),
    m_controller(nullptr)
{
    ui->setupUi(this);

    ui->comboBoxPPBSelect->clear();
    for (int i = 0; i < 16; ++i) {
        ui->comboBoxPPBSelect->addItem(QString("ППБ%1").arg(i + 1));
    }

    connect(ui->pushButtonPollStatus, &QPushButton::clicked,
            this, &ControlWidget::onPollClicked);
    connect(ui->pushButtonReset, &QPushButton::clicked,
            this, &ControlWidget::onResetClicked);
    connect(ui->pushButtonTestSequence, &QPushButton::clicked,
            this, &ControlWidget::onTestClicked);
    connect(ui->checkBoxAutoPoll, &QCheckBox::toggled,
            this, &ControlWidget::onAutoPollToggled);
    connect(ui->comboBoxPPBSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWidget::onComboBoxIndexChanged);

    // Поля ввода

    connect(ui->power_set_up_ch1, &QLineEdit::editingFinished,
            this, &ControlWidget::onPower1Changed);
    connect(ui->power_set_up_ch2, &QLineEdit::editingFinished,
            this, &ControlWidget::onPower2Changed);

    QDoubleValidator *validator = new QDoubleValidator(this);
    validator ->setLocale(QLocale::Russian);
    validator->setBottom(0.0);
    validator->setTop(10000.0);
    ui->power_set_up_ch1->setValidator(validator);
    ui->power_set_up_ch2->setValidator(validator);

    connect(ui->disactiveFu, &QCheckBox::toggled,
            this, &ControlWidget::onFuBlockedToggled);
    connect(ui->isReboot, &QCheckBox::toggled,
            this, &ControlWidget::onRebootToggled);
    connect(ui->isreseterror, &QCheckBox::toggled,
            this, &ControlWidget::onResetErrorsToggled);
    connect(ui->powerCheckBox, &QCheckBox::toggled,
            this, &ControlWidget::onPowerToggled);
}

ControlWidget::~ControlWidget()
{
    delete ui;
}

void ControlWidget::setController(PPBController* controller)
{
    if (m_controller == controller) return;

    // Отключаем старый контроллер
    if (m_controller) {
        disconnect(m_controller, &PPBController::fullStateUpdated,
                   this, &ControlWidget::onFullStateUpdated);
        disconnect(m_controller, &PPBController::userSettingsChanged,
                   this, &ControlWidget::onUserSettingsChanged);
    }

    m_controller = controller;

    if (m_controller) {
        connect(m_controller, &PPBController::fullStateUpdated,
                this, &ControlWidget::onFullStateUpdated);
        connect(m_controller, &PPBController::userSettingsChanged,
                this, &ControlWidget::onUserSettingsChanged);
        refreshSettings(); // инициализация полей
    }
}

int ControlWidget::currentPPBIndex() const
{
    return ui->comboBoxPPBSelect->currentIndex();
}

void ControlWidget::setCurrentPPBIndex(int index)
{
    if (index >= 0 && index < ui->comboBoxPPBSelect->count()) {
        ui->comboBoxPPBSelect->blockSignals(true);
        ui->comboBoxPPBSelect->setCurrentIndex(index);
        ui->comboBoxPPBSelect->blockSignals(false);
        refreshSettings();
    }
}

void ControlWidget::setBusy(bool busy)
{
    m_busy = busy;
    bool enable = !busy && m_connected;
    ui->pushButtonPollStatus->setEnabled(enable);
    ui->pushButtonReset->setEnabled(enable);
    ui->pushButtonTestSequence->setEnabled(enable);
    ui->checkBoxAutoPoll->setEnabled(enable);
    ui->comboBoxPPBSelect->setEnabled(!busy);
}

void ControlWidget::setConnected(bool connected)
{
    m_connected = connected;
    setBusy(m_busy);
}

void ControlWidget::setAutoPollChecked(bool checked)
{
    ui->checkBoxAutoPoll->blockSignals(true);
    ui->checkBoxAutoPoll->setChecked(checked);
    ui->checkBoxAutoPoll->blockSignals(false);
}

void ControlWidget::refreshSettings()
{
    if (m_controller) {
        onUserSettingsChanged(currentPPBIndex());
    }
}

// ==================== СЛОТЫ UI ====================

void ControlWidget::onPollClicked()      { emit pollStatusClicked(); }
void ControlWidget::onResetClicked()
{
    //onPower1Changed();
    //onPower2Changed();
    emit resetClicked();
}
void ControlWidget::onTestClicked()      { emit testSequenceClicked(); }
void ControlWidget::onAutoPollToggled(bool checked) { emit autoPollToggled(checked); }
void ControlWidget::onComboBoxIndexChanged(int index) { emit ppbSelected(index); }

// Редактирование полей – вызываем методы установки настроек
void ControlWidget::onPower1Changed()
{
   QString text = ui->power_set_up_ch1->text();
    bool ok;
    QLocale locale(QLocale::Russian);
    float watts = locale.toFloat(text, &ok);
    if (ok && m_controller) {
        m_controller->setPowerSetting(currentPPBIndex(), 1, watts);
    }
}

void ControlWidget::onPower2Changed()
{
      QString text = ui->power_set_up_ch2->text();
    bool ok;
    QLocale locale(QLocale::Russian);
    float watts = locale.toFloat(text, &ok);
    if (ok && m_controller) {
        m_controller->setPowerSetting(currentPPBIndex(), 2, watts);
    }
}

void ControlWidget::onFuBlockedToggled(bool checked)
{
    if (m_controller) {
        m_controller->setFuBlockedSetting(currentPPBIndex(), checked);
    }
}

void ControlWidget::onRebootToggled(bool checked)
{
    if (m_controller) {
        m_controller->setRebootRequestedSetting(currentPPBIndex(), checked);
    }
}

void ControlWidget::onResetErrorsToggled(bool checked)
{
    if (m_controller) {
        m_controller->setResetErrorsSetting(currentPPBIndex(), checked);
    }
}

void ControlWidget::onPowerToggled(bool checked)
{
    if (m_controller) {
        m_controller->setPowerEnabledSetting(currentPPBIndex(), checked);
    }
}

// ==================== СЛОТЫ ОТ КОНТРОЛЛЕРА ====================

void ControlWidget::onFullStateUpdated(uint8_t ppbIndex)
{
    Q_UNUSED(ppbIndex);
    // Реальное состояние не влияет на поля ввода – оставляем пустым
    // Если появятся индикаторы, их можно обновлять здесь
}

void ControlWidget::onUserSettingsChanged(uint8_t ppbIndex)
{
    if (ppbIndex != currentPPBIndex() || !m_controller) return;

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

    ui->powerCheckBox->blockSignals(true);
    ui->powerCheckBox->setChecked(settings.powerEnabled);
    ui->powerCheckBox->blockSignals(false);
}

uint16_t ControlWidget::getSelectedAddress() const
{
    int index = currentPPBIndex();
    if (index >= 0 && index < 16)
        return (1 << index);
    return 0;
}
