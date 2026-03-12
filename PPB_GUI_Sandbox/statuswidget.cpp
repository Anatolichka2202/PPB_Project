#include "statuswidget.h"
#include "ui_statuswidget.h"
#include "dependencies.h" //

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PPBStatus)
{
    ui->setupUi(this);

    // Инициализация индикаторов (кружков) – как было в TesterWindow::initializeUI
    QString defaultCircleStyle = "border-radius: 10px; border: 2px solid #666; background-color: #cccccc;";

    // Канал 1
    ui->Power_circle_label_ppb_chanel_1->setStyleSheet(defaultCircleStyle);
    ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(defaultCircleStyle);
    ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(defaultCircleStyle);
    ui->tem_circle_label_ppb_chanel_1->setStyleSheet(defaultCircleStyle);

    // Канал 2
    ui->Power_circle_label_ppb_chanel_2->setStyleSheet(defaultCircleStyle);
    ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(defaultCircleStyle);
    ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(defaultCircleStyle);
    ui->tem_circle_label_ppb_chanel_2->setStyleSheet(defaultCircleStyle);
    qDebug() << "Constructor of StatusWidget ";
}

StatusWidget::~StatusWidget()
{
    delete ui;
}


void StatusWidget::setDisplayMode(bool codes)
{
    m_displayAsCodes = codes;
    // Можно ничего не делать, обновление произойдёт при следующем setChannelState
}

void StatusWidget::updateState(const PPBFullState& state, bool showCodes)
{
    // Стили для индикаторов
    QString styleNormal  = "border-radius: 10px; border: 2px solid #666; background-color: #00ff00;";
    QString styleWarning = "border-radius: 10px; border: 2px solid #666; background-color: #ffff00;";
    QString styleAlarm   = "border-radius: 10px; border: 2px solid #666; background-color: #ff0000;";

    // ==================== Канал 1 ====================
    // Питание
    if (state.ch1.isOk) {
        ui->Power_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
        ui->statuslabel_power_ppb_chanel_1->setText(showCodes ? "0x01" : "В норме");
    } else {
        ui->Power_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
        ui->statuslabel_power_ppb_chanel_1->setText(showCodes ? "0x00" : "АВАРИЯ");
    }

    // Мощность
    QString powerValue = formatPower(state.ch1.power, showCodes);
    ui->statuslabel_capacity_ppb_chanel_1->setText(powerValue);
    if (state.ch1.power >= 1200.0f && state.ch1.power <= 1300.0f) {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.ch1.power >= 550.0f) {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // КСВН
    QString vswrValue = formatVSWR(state.ch1.vswr, showCodes);
    ui->statuslabel_kswn_ppb_chanel_1->setText(vswrValue);
    if (state.ch1.vswr <= 1.3f) {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.ch1.vswr <= 4.0f) {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // Температуры канала 1
    QString temp1Value = formatTemperature(state.tempT1, showCodes);
    ui->statuslabel_temp_ppb_chanel_1->setText(temp1Value);
    if (state.tempT1 <= 70.0f) {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.tempT1 <= 85.0f) {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // Дополнительные температуры
    ui->statuslabel_temp_3_ppb_chanel_1->setText(formatTemperature(state.tempT3, showCodes));
    ui->statuslabel_tempv1_ppb_chanel_1->setText(formatTemperature(state.tempV1, showCodes));
    ui->statuslabel_temp_in_ppb_chanel_1->setText(formatTemperature(state.tempIn, showCodes));

    // ==================== Канал 2 ====================
    // Питание
    if (state.ch2.isOk) {
        ui->Power_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
        ui->statuslabel_power_ppb_chanel_2->setText(showCodes ? "0x01" : "В норме");
    } else {
        ui->Power_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
        ui->statuslabel_power_ppb_chanel_2->setText(showCodes ? "0x00" : "АВАРИЯ");
    }

    // Мощность
    powerValue = formatPower(state.ch2.power, showCodes);
    ui->statuslabel_capacity_ppb_chanel_2->setText(powerValue);
    if (state.ch2.power >= 1200.0f && state.ch2.power <= 1300.0f) {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.ch2.power >= 550.0f) {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // КСВН
    vswrValue = formatVSWR(state.ch2.vswr, showCodes);
    ui->statuslabel_kswn_ppb_chanel_2->setText(vswrValue);
    if (state.ch2.vswr <= 1.3f) {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.ch2.vswr <= 4.0f) {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // Температуры канала 2
    QString temp2Value = formatTemperature(state.tempT2, showCodes);
    ui->statuslabel_temp_ppb_chanel_2->setText(temp2Value);
    if (state.tempT2 <= 70.0f) {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.tempT2 <= 85.0f) {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // Дополнительные температуры
    ui->statuslabel_temp_4_ppb_chanel_2->setText(formatTemperature(state.tempT4, showCodes));
    ui->statuslabel_tempv2_ppb_chanel_2->setText(formatTemperature(state.tempV2, showCodes));
    ui->statuslabel_temp_out_ppb_chanel_2->setText(formatTemperature(state.tempOut, showCodes));
}

QString StatusWidget::formatPower(float watts, bool codes) const
{
    if (codes) {
        uint16_t code = DataConverter::powerToCode(watts);
        return QString("0x%1").arg(code, 4, 16, QChar('0')).toUpper();
    } else {
        return QString("%1 Вт").arg(watts, 0, 'f', 1);
    }
}

QString StatusWidget::formatTemperature(float celsius, bool codes) const
{
    if (codes) {
        int16_t code = DataConverter::temperatureToCode(celsius);
        return QString("0x%1").arg(static_cast<uint16_t>(code), 4, 16, QChar('0')).toUpper();
    } else {
        return QString("%1°C").arg(celsius, 0, 'f', 1);
    }
}

QString StatusWidget::formatVSWR(float vswr, bool codes) const
{
    if (codes) {
        uint16_t code = DataConverter::vswrToCode(vswr);
        return QString("0x%1").arg(code, 4, 16, QChar('0')).toUpper();
    } else {
        return QString("%1").arg(vswr, 0, 'f', 2);
    }
}
