#include "akip_pult.h"
#include "ui_akip_pult.h"

akip_pult::akip_pult(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::akip_pult)
{
    ui->setupUi(this);
    qDebug() << "Constructor of AkipPult";
}

akip_pult::~akip_pult()
{
    delete ui;
}
