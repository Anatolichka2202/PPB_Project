#include "fuwidget.h"
#include "ui_fuwidget.h"
#include <logmacros.h>

FuWidget::FuWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FuWidget)
{
    ui->setupUi(this);
    ui->radioButtonFUReceive->setChecked(true);
    ui->lineEditPulseDuration->setText("27000");
    ui->lineEditPulseDelay->setText("0");

    connect(ui->radioButtonFUTransmit, &QRadioButton::toggled,
            this, &FuWidget::onTransmitToggled);
    connect(ui->radioButtonFUReceive, &QRadioButton::toggled,
            this, &FuWidget::onReceiveToggled);
    qDebug() << "Constructor of FUWidget";
}

FuWidget::~FuWidget()
{
    delete ui;
}

bool FuWidget::isTransmitMode() const
{
    return ui->radioButtonFUTransmit->isChecked();
}

uint32_t FuWidget::pulseDuration() const
{
    return ui->lineEditPulseDuration->text().toUInt();
}

uint32_t FuWidget::pulseDelay() const
{
    return ui->lineEditPulseDelay->text().toUInt();
}

void FuWidget::onTransmitToggled(bool checked)
{
    if (checked) emit modeChanged(true);
}

void FuWidget::onReceiveToggled(bool checked)
{
    if (checked) emit modeChanged(false);
}

void FuWidget::on_fuBtnSent_clicked()
{
    bool ok1, ok2;
    uint32_t dur = ui->lineEditPulseDuration->text().toUInt(&ok1);
    uint32_t duty = ui->lineEditPulseDelay->text().toUInt(&ok2);

    if (!ok1 || !ok2) {
        LOG_UI_ALERT("Ошибка! Введите целые числа в поля длительности и задержки");
        return;
    }

    if (dur < 1 || dur > 30 || duty < 1 || duty > 30) {
        LOG_UI_ALERT("Ошибка! Значения должны быть от 1 до 30 мкс");
        return;
    }

    bool transmit = ui->radioButtonFUTransmit->isChecked();
    emit sendFuCommand(transmit, static_cast<uint16_t>(dur), static_cast<uint16_t>(duty));
}

