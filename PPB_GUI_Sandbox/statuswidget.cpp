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

void StatusWidget::setChannel1State(const UIChannelState& state, bool showCodes)
{
    if (!ui) {
        LOG_UI_ALERT("StatusWidget::setChannel1State: ui is null");
        return;
    }
    updateChannel(1, state, showCodes);
}

void StatusWidget::setChannel2State(const UIChannelState& state, bool showCodes)
{
    updateChannel(2, state, showCodes);
}

void StatusWidget::setDisplayMode(bool codes)
{
    m_displayAsCodes = codes;
    // Можно ничего не делать, обновление произойдёт при следующем setChannelState
}

void StatusWidget::updateChannel(int channel, const UIChannelState& state, bool codes)
{
    // Выбираем нужные виджеты по суффиксу
    QString suffix = QString::number(channel);
    QLabel* powerCircle = findChild<QLabel*>("Power_circle_label_ppb_chanel_" + suffix);
    QLabel* powerText   = findChild<QLabel*>("statuslabel_power_ppb_chanel_" + suffix);
    QLabel* capCircle   = findChild<QLabel*>("Capacity_circle_label_ppb_chanel_" + suffix);
    QLabel* capText     = findChild<QLabel*>("statuslabel_capacity_ppb_chanel_" + suffix);
    QLabel* vswrCircle  = findChild<QLabel*>("KSWN_circle_label_ppb_chanel_" + suffix);
    QLabel* vswrText    = findChild<QLabel*>("statuslabel_kswn_ppb_chanel_" + suffix);
    QLabel* tempCircle  = findChild<QLabel*>("tem_circle_label_ppb_chanel_" + suffix);
    QLabel* tempText    = findChild<QLabel*>("statuslabel_temp_ppb_chanel_" + suffix);

    if (!powerCircle || !powerText || !capCircle || !capText || !vswrCircle || !vswrText || !tempCircle || !tempText)
        return;

    // Стили для индикаторов
    QString styleNormal  = "border-radius: 10px; border: 2px solid #666; background-color: #00ff00;";
    QString styleWarning = "border-radius: 10px; border: 2px solid #666; background-color: #ffff00;";
    QString styleAlarm   = "border-radius: 10px; border: 2px solid #666; background-color: #ff0000;";

    // Питание
    if (state.isOk) {
        powerCircle->setStyleSheet(styleNormal);
        powerText->setText(codes ? "0x01" : "В норме");
    } else {
        powerCircle->setStyleSheet(styleAlarm);
        powerText->setText(codes ? "0x00" : "АВАРИЯ");
    }

    // Мощность
    QString powerValue = formatPower(state.power, codes);
    if (state.power >= 1200.0f && state.power <= 1300.0f) {
        capCircle->setStyleSheet(styleNormal);
    } else if (state.power >= 550.0f) {
        capCircle->setStyleSheet(styleWarning);
    } else {
        capCircle->setStyleSheet(styleAlarm);
    }
    capText->setText(powerValue);

    // КСВН
    QString vswrValue = formatVSWR(state.vswr, codes);
    if (state.vswr <= 1.3f) {
        vswrCircle->setStyleSheet(styleNormal);
    } else if (state.vswr <= 4.0f) {
        vswrCircle->setStyleSheet(styleWarning);
    } else {
        vswrCircle->setStyleSheet(styleAlarm);
    }
    vswrText->setText(vswrValue);

    // Температура
    QString tempValue = formatTemperature(state.temperature, codes);
    if (state.temperature <= 70.0f) {
        tempCircle->setStyleSheet(styleNormal);
    } else if (state.temperature <= 85.0f) {
        tempCircle->setStyleSheet(styleWarning);
    } else {
        tempCircle->setStyleSheet(styleAlarm);
    }
    tempText->setText(tempValue);

    if (channel == 1) {
        if (QLabel* t3 = findChild<QLabel*>("statuslabel_temp_3_ppb_chanel_1"))
            t3->setText(formatTemperature(state.tempT3, codes));
        if (QLabel* v1 = findChild<QLabel*>("statuslabel_tempv1_ppb_chanel_1"))
            v1->setText(formatTemperature(state.tempV1, codes));
        if (QLabel* tin = findChild<QLabel*>("statuslabel_temp_in_ppb_chanel_1"))
            tin->setText(formatTemperature(state.tempIn, codes));
    } else {
        if (QLabel* t4 = findChild<QLabel*>("statuslabel_temp_4_ppb_chanel_2"))
            t4->setText(formatTemperature(state.tempT4, codes));
        if (QLabel* v2 = findChild<QLabel*>("statuslabel_tempv2_ppb_chanel_2"))
            v2->setText(formatTemperature(state.tempV2, codes));
        if (QLabel* tout = findChild<QLabel*>("statuslabel_temp_out_ppb_chanel_2"))
            tout->setText(formatTemperature(state.tempOut, codes));
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
