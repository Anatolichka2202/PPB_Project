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
    ui->lineEditPulseDelay->setText("30");

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
    // dur - длительность единицы, duty - скважность в процентах.
    // Полный период = dur / duty * 100, длительность нуля = полный период - dur.
    float zeroDuration = (dur / static_cast<float>(duty)) * 100.0f - dur;
    if (zeroDuration >= 65536.0f) {
        QMessageBox::warning(this, "Ошибка", (QString("Значение периода нуля слишком большое = %1")
                                                   .arg(zeroDuration)));
        LOG_UI_ALERT(QString("Значение периода нуля слишком большое = %1")
                         .arg(zeroDuration));
        return;
    }

    int zeroDurationInt = static_cast<int>(zeroDuration);
    float fraction = (zeroDuration - zeroDurationInt) * 100.0f;

    uint8_t arr[3]; // [целая часть hi][целая часть lo][сотые доли]
    arr[0] = static_cast<uint8_t>((static_cast<uint16_t>(zeroDurationInt) >> 8) & 0xFF);
    arr[1] = static_cast<uint8_t>(zeroDurationInt & 0xFF);
    arr[2] = static_cast<uint8_t>(fraction);

    emit sendFuCommand(transmit, static_cast<uint16_t>(dur), arr);
}
