#include "statuswidget.h"
#include "ui_statuswidget.h"
#include "dependencies.h"

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
    QString styleUnknown = "border-radius: 10px; border: 2px solid #666; background-color: #cccccc;";

    // ==================== Канал 1 ====================
    // Питание (isOk)
    if (state.ch1.isOk) {
        ui->Power_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
        ui->statuslabel_power_ppb_chanel_1->setText(showCodes ? "0x01" : "В норме");
    } else {
        ui->Power_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
        ui->statuslabel_power_ppb_chanel_1->setText(showCodes ? "0x00" : "АВАРИЯ");
    }

    // Мощность
    if (showCodes) {
        ui->statuslabel_capacity_ppb_chanel_1->setText(
            QString("0x%1").arg(state.ch1.powerCode, 8, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_capacity_ppb_chanel_1->setText(
            QString("%1 Вт").arg(state.ch1.power, 0, 'f', 1));
    }
    // Индикатор мощности (по физическому значению)
    if (state.ch1.power >= 1200.0f && state.ch1.power <= 1300.0f) {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.ch1.power >= 550.0f) {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->Capacity_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // КСВН
    if (showCodes) {
        ui->statuslabel_kswn_ppb_chanel_1->setText(
            QString("0x%1").arg(state.ch1.vswrCode, 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_kswn_ppb_chanel_1->setText(
            QString::number(state.ch1.vswr, 'f', 2));
    }
    if (state.ch1.vswr <= 1.3f) {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.ch1.vswr <= 4.0f) {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->KSWN_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // Температура t1
    if (showCodes) {
        ui->statuslabel_temp_ppb_chanel_1->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempT1Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_ppb_chanel_1->setText(
            QString("%1°C").arg(state.tempT1, 0, 'f', 1));
    }
    if (state.tempT1 <= 70.0f) {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleNormal);
    } else if (state.tempT1 <= 85.0f) {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_chanel_1->setStyleSheet(styleAlarm);
    }

    // Температура t3
    if (showCodes) {
        ui->statuslabel_temp_3_ppb_chanel_1->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempT3Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_3_ppb_chanel_1->setText(
            QString("%1°C").arg(state.tempT3, 0, 'f', 1));
    }
    // Индикатор для t3 (используем тот же порог 70/85)
    if (state.tempT3 <= 70.0f) {
        ui->tem_circle_label_ppb_bp->setStyleSheet(styleNormal);
    } else if (state.tempT3 <= 85.0f) {
        ui->tem_circle_label_ppb_bp->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_bp->setStyleSheet(styleAlarm);
    }

    // Температура v1
    if (showCodes) {
        ui->statuslabel_tempv1_ppb_chanel_1->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempV1Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_tempv1_ppb_chanel_1->setText(
            QString("%1°C").arg(state.tempV1, 0, 'f', 1));
    }
    if (state.tempV1 <= 70.0f) {
        ui->tem_circle_label_ppb_v1->setStyleSheet(styleNormal);
    } else if (state.tempV1 <= 85.0f) {
        ui->tem_circle_label_ppb_v1->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_v1->setStyleSheet(styleAlarm);
    }

    // Температура t_in
    if (showCodes) {
        ui->statuslabel_temp_in_ppb_chanel_1->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempInCode), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_in_ppb_chanel_1->setText(
            QString("%1°C").arg(state.tempIn, 0, 'f', 1));
    }
    if (state.tempIn <= 70.0f) {
        ui->tem_circle_label_ppb_in->setStyleSheet(styleNormal);
    } else if (state.tempIn <= 85.0f) {
        ui->tem_circle_label_ppb_in->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_in->setStyleSheet(styleAlarm);
    }

    // ==================== Канал 2 ====================
    // Питание (isOk)
    if (state.ch2.isOk) {
        ui->Power_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
        ui->statuslabel_power_ppb_chanel_2->setText(showCodes ? "0x01" : "В норме");
    } else {
        ui->Power_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
        ui->statuslabel_power_ppb_chanel_2->setText(showCodes ? "0x00" : "АВАРИЯ");
    }

    // Мощность
    if (showCodes) {
        ui->statuslabel_capacity_ppb_chanel_2->setText(
            QString("0x%1").arg(state.ch2.powerCode, 8, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_capacity_ppb_chanel_2->setText(
            QString("%1 Вт").arg(state.ch2.power, 0, 'f', 1));
    }
    if (state.ch2.power >= 1200.0f && state.ch2.power <= 1300.0f) {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.ch2.power >= 550.0f) {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->Capacity_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // КСВН
    if (showCodes) {
        ui->statuslabel_kswn_ppb_chanel_2->setText(
            QString("0x%1").arg(state.ch2.vswrCode, 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_kswn_ppb_chanel_2->setText(
            QString::number(state.ch2.vswr, 'f', 2));
    }
    if (state.ch2.vswr <= 1.3f) {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.ch2.vswr <= 4.0f) {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->KSWN_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // Температура t2
    if (showCodes) {
        ui->statuslabel_temp_ppb_chanel_2->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempT2Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_ppb_chanel_2->setText(
            QString("%1°C").arg(state.tempT2, 0, 'f', 1));
    }
    if (state.tempT2 <= 70.0f) {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleNormal);
    } else if (state.tempT2 <= 85.0f) {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_chanel_2->setStyleSheet(styleAlarm);
    }

    // Температура t4
    if (showCodes) {
        ui->statuslabel_temp_4_ppb_chanel_2->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempT4Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_4_ppb_chanel_2->setText(
            QString("%1°C").arg(state.tempT4, 0, 'f', 1));
    }
    if (state.tempT4 <= 70.0f) {
        ui->tem_circle_label_ppb_bp2->setStyleSheet(styleNormal);
    } else if (state.tempT4 <= 85.0f) {
        ui->tem_circle_label_ppb_bp2->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_bp2->setStyleSheet(styleAlarm);
    }

    // Температура v2
    if (showCodes) {
        ui->statuslabel_tempv2_ppb_chanel_2->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempV2Code), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_tempv2_ppb_chanel_2->setText(
            QString("%1°C").arg(state.tempV2, 0, 'f', 1));
    }
    if (state.tempV2 <= 70.0f) {
        ui->tem_circle_label_ppb_v2->setStyleSheet(styleNormal);
    } else if (state.tempV2 <= 85.0f) {
        ui->tem_circle_label_ppb_v2->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_v2->setStyleSheet(styleAlarm);
    }

    // Температура t_out
    if (showCodes) {
        ui->statuslabel_temp_out_ppb_chanel_2->setText(
            QString("0x%1").arg(static_cast<uint16_t>(state.tempOutCode), 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_temp_out_ppb_chanel_2->setText(
            QString("%1°C").arg(state.tempOut, 0, 'f', 1));
    }
    if (state.tempOut <= 70.0f) {
        ui->tem_circle_label_ppb_out->setStyleSheet(styleNormal);
    } else if (state.tempOut <= 85.0f) {
        ui->tem_circle_label_ppb_out->setStyleSheet(styleWarning);
    } else {
        ui->tem_circle_label_ppb_out->setStyleSheet(styleAlarm);
    }
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
