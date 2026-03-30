#include "fuwidget.h"
#include "ui_fuwidget.h"
#include <logmacros.h>
#include <QMessageBox>
FuWidget::FuWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FuWidget)
{
    ui->setupUi(this);
    ui->radioButtonFUReceive->setChecked(true);
    ui->lineEditPulseDuration->setText("100");
    ui->lineEditPulseDelay->setText("200");

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

    if (dur < 1 || dur > 27000 || duty < 1 || duty > 99) {
        QMessageBox::warning(this, "Ошибка", "Значения периода должны быть от 1 до 27000 мкс, значение скважности от 1 до 99");
        LOG_UI_ALERT("Значения периода должны быть от 1 до 27000 мкс, значение скважности от 1 до 99");
        return;
    }

    bool transmit = ui->radioButtonFUTransmit->isChecked();
    //dur - период, duty - скважность
    float delay = (dur/static_cast<float> (duty)) * 100; // считаем длительность
    emit sendFuCommand(transmit, static_cast<uint16_t>(dur), static_cast<uint16_t>(delay - dur));
}

