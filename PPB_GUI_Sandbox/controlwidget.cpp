#include "controlwidget.h"
#include "ui_controlwidget.h"
#include "dataconverter.h"

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
    connect(ui->pushButtonPollStatus, &QPushButton::clicked, this, &ControlWidget::onPollClicked);
    connect(ui->pushButtonReset, &QPushButton::clicked, this, &ControlWidget::onResetClicked);
    connect(ui->pushButtonTestSequence, &QPushButton::clicked, this, &ControlWidget::onTestClicked);
    connect(ui->checkBoxAutoPoll, &QCheckBox::toggled, this, &ControlWidget::onAutoPollToggled);
    connect(ui->comboBoxPPBSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWidget::onComboBoxIndexChanged);

    // Поля ввода
    connect(ui->power_set_up_ch1, &QLineEdit::textChanged, this, &ControlWidget::onPower1Changed);
    connect(ui->power_set_up_ch2, &QLineEdit::textChanged, this, &ControlWidget::onPower2Changed);
    connect(ui->disactiveFu, &QCheckBox::toggled, this, &ControlWidget::onFuBlockedToggled);
    connect(ui->isReboot, &QCheckBox::toggled, this, &ControlWidget::onRebootToggled);
    connect(ui->isreseterror, &QCheckBox::toggled, this, &ControlWidget::onResetErrorsToggled);

    connect(ui->powerCheckBox, &QCheckBox::toggled,
            this, &ControlWidget::onPowerToggled);

    if (m_controller) {
        connect(m_controller, &PPBController::fullStateUpdated, this, &ControlWidget::onFullStateUpdated);
    }
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
    }

    m_controller = controller;

    // Подключаем новый
    if (m_controller) {
        connect(m_controller, &PPBController::fullStateUpdated,
                this, &ControlWidget::onFullStateUpdated);
        // Инициализируем поля текущим состоянием
        onFullStateUpdated(currentPPBIndex());
    }
}

int ControlWidget::currentPPBIndex() const
{
    return ui->comboBoxPPBSelect->currentIndex();
}

void ControlWidget::setCurrentPPBIndex(int index)
{
    if (index >= 0 && index < ui->comboBoxPPBSelect->count())
        ui->comboBoxPPBSelect->setCurrentIndex(index);
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

void ControlWidget::onPollClicked()      { emit pollStatusClicked(); }
void ControlWidget::onResetClicked()     { emit resetClicked(); }
void ControlWidget::onTestClicked()      { emit testSequenceClicked(); }
void ControlWidget::onAutoPollToggled(bool checked) { emit autoPollToggled(checked); }
void ControlWidget::onComboBoxIndexChanged(int index) { emit ppbSelected(index); }

void ControlWidget::onPower1Changed(const QString& text)
{
    bool ok;
    QString normalized = QString(text).replace(',', '.');
    float watts = normalized.toFloat(&ok);

    if (ok && m_controller) {
        m_controller->setChannelPower(currentPPBIndex(), 1, watts);
    }
}

void ControlWidget::onPower2Changed(const QString& text)
{
    bool ok;
    float watts = text.toFloat(&ok);
    if (ok && m_controller) {
        m_controller->setChannelPower(currentPPBIndex(), 2, watts);
    }
}

void ControlWidget::onFuBlockedToggled(bool checked)
{
    if (m_controller) m_controller->setFuBlocked(currentPPBIndex(), checked);
}

void ControlWidget::onRebootToggled(bool checked)
{
    if (m_controller) m_controller->setRebootRequested(currentPPBIndex(), checked);
}

void ControlWidget::onResetErrorsToggled(bool checked)
{
    if (m_controller) m_controller->setResetErrors(currentPPBIndex(), checked);
}

void ControlWidget::onFullStateUpdated(uint8_t ppbIndex)
{
    if (ppbIndex != currentPPBIndex()) return;
    auto state = m_controller->getFullState(ppbIndex);

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

    ui->powerCheckBox->blockSignals(true);
    ui->powerCheckBox->setChecked(state.powerEnabled);
    ui->powerCheckBox->blockSignals(false);
}

uint16_t ControlWidget::getSelectedAddress() const
{
    int index = currentPPBIndex();
    if (index >= 0 && index < 16)
        return (1 << index);
    return 0;
}

void ControlWidget::onPowerToggled(bool checked)
{
    if (m_controller) {
        m_controller->setPowerEnabled(currentPPBIndex(), checked);
    }
}
