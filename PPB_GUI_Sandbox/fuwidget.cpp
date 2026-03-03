#include "fuwidget.h"
#include "ui_fuwidget.h"

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
