#include "controlwidget.h"
#include "ui_controlwidget.h"

ControlWidget::ControlWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    // Заполняем комбобокс на 16 ППБ
    ui->comboBoxPPBSelect->clear();
    for (int i = 0; i < 16; ++i) {
        ui->comboBoxPPBSelect->addItem(QString("ППБ%1").arg(i + 1));
    }

    // Подключаем внутренние сигналы
    connect(ui->pushButtonPollStatus, &QPushButton::clicked,
            this, &ControlWidget::onPollClicked);

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
    qDebug() << "Constructor of ControlWidget";
}

ControlWidget::~ControlWidget()
{
    delete ui;
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
    ui->comboBoxPPBSelect->setEnabled(!busy);   // комбобокс можно переключать даже без подключения
}

void ControlWidget::setConnected(bool connected)
{
    m_connected = connected;
    setBusy(m_busy);   // пересчитает доступность кнопок
}

void ControlWidget::setAutoPollChecked(bool checked)
{
    // Блокируем сигналы, чтобы не вызвать повторную команду
    ui->checkBoxAutoPoll->blockSignals(true);
    ui->checkBoxAutoPoll->setChecked(checked);
    ui->checkBoxAutoPoll->blockSignals(false);
}

void ControlWidget::onPollClicked()      { emit pollStatusClicked(); }
void ControlWidget::onResetClicked()     { emit resetClicked(); }
void ControlWidget::onTestClicked()      { emit testSequenceClicked(); }
void ControlWidget::onAutoPollToggled(bool checked) { emit autoPollToggled(checked); }
void ControlWidget::onComboBoxIndexChanged(int index) { emit ppbSelected(index); }




