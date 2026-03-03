#include "dataviewwidget.h"
#include "ui_dataviewwidget.h"

DataViewWidget::DataViewWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataViewWidget)
{
    ui->setupUi(this);
    ui->radioButtonShowPhysical->setChecked(true);   // по умолчанию физические

    connect(ui->radioButtonShowPhysical, &QRadioButton::toggled,
            this, &DataViewWidget::onPhysicalToggled);
    connect(ui->radioButtonShowCodes, &QRadioButton::toggled,
            this, &DataViewWidget::onCodesToggled);
    qDebug() << "Constructor of dataviewWidget";
}

DataViewWidget::~DataViewWidget()
{
    delete ui;
}

bool DataViewWidget::showCodes() const
{
    return ui->radioButtonShowCodes->isChecked();
}

void DataViewWidget::onPhysicalToggled(bool checked)
{
    if (checked) emit displayModeChanged(false);
}

void DataViewWidget::onCodesToggled(bool checked)
{
    if (checked) emit displayModeChanged(true);
}
