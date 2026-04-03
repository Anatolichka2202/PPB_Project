#include "statuswidget.h"
#include "ui_statuswidget.h"
#include "dependencies.h"
#include <QStyle>
// Вспомогательная функция для установки состояния индикатора
static void setIndicatorState(QLabel* indicator, const QString& state)
{
    if (!indicator) return;
    indicator->setProperty("state", state);
    indicator->style()->polish(indicator);
}

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PPBStatus)
{
    ui->setupUi(this);

    // Список всех лампочек-индикаторов (кружков)
    QList<QLabel*> indicators = {
        ui->Power_circle_label_ppb_chanel_1,
        ui->Capacity_circle_label_ppb_chanel_1,
        ui->KSWN_circle_label_ppb_chanel_1,
        ui->tem_circle_label_ppb_chanel_1,
        ui->tem_circle_label_ppb_bp,
        ui->tem_circle_label_ppb_v1,
        ui->tem_circle_label_ppb_in,
        ui->Power_circle_label_ppb_chanel_2,
        ui->Capacity_circle_label_ppb_chanel_2,
        ui->KSWN_circle_label_ppb_chanel_2,
        ui->tem_circle_label_ppb_chanel_2,
        ui->tem_circle_label_ppb_bp2,
        ui->tem_circle_label_ppb_v2,
        ui->tem_circle_label_ppb_out
    };

    // Устанавливаем тип и начальное состояние для каждой лампочки
    for (QLabel* lbl : indicators) {
        lbl->setProperty("type", "indicator");
        setIndicatorState(lbl, "unknown");
    }

    // Для текстовых значений (чтобы можно было стилизовать отдельно)
    QList<QLabel*> valueLabels = {
        ui->statuslabel_power_ppb_chanel_1,
        ui->statuslabel_capacity_ppb_chanel_1,
        ui->statuslabel_kswn_ppb_chanel_1,
        ui->statuslabel_temp_ppb_chanel_1,
        ui->statuslabel_temp_3_ppb_chanel_1,
        ui->statuslabel_tempv1_ppb_chanel_1,
        ui->statuslabel_temp_in_ppb_chanel_1,
        ui->statuslabel_power_ppb_chanel_2,
        ui->statuslabel_capacity_ppb_chanel_2,
        ui->statuslabel_kswn_ppb_chanel_2,
        ui->statuslabel_temp_ppb_chanel_2,
        ui->statuslabel_temp_4_ppb_chanel_2,
        ui->statuslabel_tempv2_ppb_chanel_2,
        ui->statuslabel_temp_out_ppb_chanel_2
    };
    for (QLabel* lbl : valueLabels) {
        lbl->setProperty("type", "value");
    }
}

StatusWidget::~StatusWidget()
{
    delete ui;
}

void StatusWidget::setDisplayMode(bool codes)
{
    m_displayAsCodes = codes;
    // При смене режима обновление произойдёт при следующем вызове updateState
}

void StatusWidget::updateState(const PPBFullState& state, bool showCodes)
{
    // ==================== Канал 1 ====================
    // Питание
    setIndicatorState(ui->Power_circle_label_ppb_chanel_1, state.ch1.isOk ? "ok" : "alarm");
    ui->statuslabel_power_ppb_chanel_1->setText(showCodes ? "0x01" : (state.ch1.isOk ? "В норме" : "АВАРИЯ"));

    // Мощность
    if (showCodes) {
        ui->statuslabel_capacity_ppb_chanel_1->setText(QString("0x%1").arg(state.ch1.powerCode, 8, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_capacity_ppb_chanel_1->setText(QString("%1 Вт").arg(state.ch1.power, 0, 'f', 1));
    }
    if (state.ch1.power >= 1200.0f && state.ch1.power <= 1300.0f)
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_1, "ok");
    else if (state.ch1.power >= 550.0f)
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_1, "warning");
    else
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_1, "alarm");

    // КСВН
    if (showCodes) {
        ui->statuslabel_kswn_ppb_chanel_1->setText(QString("0x%1").arg(state.ch1.vswrCode, 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_kswn_ppb_chanel_1->setText(QString::number(state.ch1.vswr, 'f', 2));
    }
    if (state.ch1.vswr <= 1.3f)
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_1, "ok");
    else if (state.ch1.vswr <= 4.0f)
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_1, "warning");
    else
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_1, "alarm");

    // Температура t1
    if (showCodes) {
        ui->statuslabel_temp_ppb_chanel_1->setText(QString("0x%1").arg(state.tempT1Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempT1 == 0 && state.tempT1Code == 0)
            ui->statuslabel_temp_ppb_chanel_1->setText("н/д");
        else
            ui->statuslabel_temp_ppb_chanel_1->setText(QString("%1°C").arg(state.tempT1, 0, 'f', 1));
    }
    if (state.tempT1 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_chanel_1, "ok");
    else if (state.tempT1 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_chanel_1, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_chanel_1, "alarm");

    // Температура t3
    if (showCodes) {
        ui->statuslabel_temp_3_ppb_chanel_1->setText(QString("0x%1").arg(state.tempT3Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempT3 == 0 && state.tempT3Code == 0)
            ui->statuslabel_temp_3_ppb_chanel_1->setText("н/д");
        else
            ui->statuslabel_temp_3_ppb_chanel_1->setText(QString("%1°C").arg(state.tempT3, 0, 'f', 1));
    }
    if (state.tempT3 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_bp, "ok");
    else if (state.tempT3 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_bp, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_bp, "alarm");

    // Температура v1
    if (showCodes) {
        ui->statuslabel_tempv1_ppb_chanel_1->setText(QString("0x%1").arg(state.tempV1Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempV1 == 0 && state.tempV1Code == 0)
            ui->statuslabel_tempv1_ppb_chanel_1->setText("н/д");
        else
            ui->statuslabel_tempv1_ppb_chanel_1->setText(QString("%1°C").arg(state.tempV1, 0, 'f', 1));
    }
    if (state.tempV1 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_v1, "ok");
    else if (state.tempV1 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_v1, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_v1, "alarm");

    // Температура t_in
    if (showCodes) {
        ui->statuslabel_temp_in_ppb_chanel_1->setText(QString("0x%1").arg(state.tempInCode, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempIn == 0 && state.tempInCode == 0)
            ui->statuslabel_temp_in_ppb_chanel_1->setText("н/д");
        else
            ui->statuslabel_temp_in_ppb_chanel_1->setText(QString("%1°C").arg(state.tempIn, 0, 'f', 1));
    }
    if (state.tempIn <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_in, "ok");
    else if (state.tempIn <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_in, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_in, "alarm");

    // ==================== Канал 2 ====================
    // Питание
    setIndicatorState(ui->Power_circle_label_ppb_chanel_2, state.ch2.isOk ? "ok" : "alarm");
    ui->statuslabel_power_ppb_chanel_2->setText(showCodes ? "0x01" : (state.ch2.isOk ? "В норме" : "АВАРИЯ"));

    // Мощность
    if (showCodes) {
        ui->statuslabel_capacity_ppb_chanel_2->setText(QString("0x%1").arg(state.ch2.powerCode, 8, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_capacity_ppb_chanel_2->setText(QString("%1 Вт").arg(state.ch2.power, 0, 'f', 1));
    }
    if (state.ch2.power >= 1200.0f && state.ch2.power <= 1300.0f)
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_2, "ok");
    else if (state.ch2.power >= 550.0f)
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_2, "warning");
    else
        setIndicatorState(ui->Capacity_circle_label_ppb_chanel_2, "alarm");

    // КСВН
    if (showCodes) {
        ui->statuslabel_kswn_ppb_chanel_2->setText(QString("0x%1").arg(state.ch2.vswrCode, 4, 16, QChar('0')).toUpper());
    } else {
        ui->statuslabel_kswn_ppb_chanel_2->setText(QString::number(state.ch2.vswr, 'f', 2));
    }
    if (state.ch2.vswr <= 1.3f)
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_2, "ok");
    else if (state.ch2.vswr <= 4.0f)
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_2, "warning");
    else
        setIndicatorState(ui->KSWN_circle_label_ppb_chanel_2, "alarm");

    // Температура t2
    if (showCodes) {
        ui->statuslabel_temp_ppb_chanel_2->setText(QString("0x%1").arg(state.tempT2Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempT2 == 0 && state.tempT2Code == 0)
            ui->statuslabel_temp_ppb_chanel_2->setText("н/д");
        else
            ui->statuslabel_temp_ppb_chanel_2->setText(QString("%1°C").arg(state.tempT2, 0, 'f', 1));
    }
    if (state.tempT2 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_chanel_2, "ok");
    else if (state.tempT2 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_chanel_2, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_chanel_2, "alarm");

    // Температура t4
    if (showCodes) {
        ui->statuslabel_temp_4_ppb_chanel_2->setText(QString("0x%1").arg(state.tempT4Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempT4 == 0 && state.tempT4Code == 0)
            ui->statuslabel_temp_4_ppb_chanel_2->setText("н/д");
        else
            ui->statuslabel_temp_4_ppb_chanel_2->setText(QString("%1°C").arg(state.tempT4, 0, 'f', 1));
    }
    if (state.tempT4 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_bp2, "ok");
    else if (state.tempT4 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_bp2, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_bp2, "alarm");

    // Температура v2
    if (showCodes) {
        ui->statuslabel_tempv2_ppb_chanel_2->setText(QString("0x%1").arg(state.tempV2Code, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempV2 == 0 && state.tempV2Code == 0)
            ui->statuslabel_tempv2_ppb_chanel_2->setText("н/д");
        else
            ui->statuslabel_tempv2_ppb_chanel_2->setText(QString("%1°C").arg(state.tempV2, 0, 'f', 1));
    }
    if (state.tempV2 <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_v2, "ok");
    else if (state.tempV2 <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_v2, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_v2, "alarm");

    // Температура t_out
    if (showCodes) {
        ui->statuslabel_temp_out_ppb_chanel_2->setText(QString("0x%1").arg(state.tempOutCode, 4, 16, QChar('0')).toUpper());
    } else {
        if (state.tempOut == 0 && state.tempOutCode == 0)
            ui->statuslabel_temp_out_ppb_chanel_2->setText("н/д");
        else
            ui->statuslabel_temp_out_ppb_chanel_2->setText(QString("%1°C").arg(state.tempOut, 0, 'f', 1));
    }
    if (state.tempOut <= 70.0f)
        setIndicatorState(ui->tem_circle_label_ppb_out, "ok");
    else if (state.tempOut <= 85.0f)
        setIndicatorState(ui->tem_circle_label_ppb_out, "warning");
    else
        setIndicatorState(ui->tem_circle_label_ppb_out, "alarm");
}
