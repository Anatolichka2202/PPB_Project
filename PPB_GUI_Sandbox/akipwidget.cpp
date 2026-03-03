#include "akipwidget.h"
#include "ui_akipwidget.h"

AkipWidget::AkipWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AkipWidget)
{
    ui->setupUi(this);
    connect(ui->pushButtonApply, &QPushButton::clicked, this, &AkipWidget::applyClicked);
    // Установка значений по умолчанию (из задания)
    ui->editFreqA->setText("500");
    ui->editPowerA->setText("0");
    ui->chkOutputA->setChecked(false);
    ui->editFreqB->setText("0");
    ui->editAmplB->setText("4.0");
    ui->editDutyB->setText("33.333");
    ui->chkOutputB->setChecked(false);
}

AkipWidget::~AkipWidget()
{
    delete ui;
}

double AkipWidget::frequencyA() const { return ui->editFreqA->text().toDouble(); }
double AkipWidget::powerA() const { return ui->editPowerA->text().toDouble(); }
bool AkipWidget::outputA() const { return ui->chkOutputA->isChecked(); }

double AkipWidget::frequencyB() const { return ui->editFreqB->text().toDouble(); }
double AkipWidget::amplitudeB() const { return ui->editAmplB->text().toDouble(); }
double AkipWidget::dutyCycleB() const { return ui->editDutyB->text().toDouble(); }
bool AkipWidget::outputB() const { return ui->chkOutputB->isChecked(); }
