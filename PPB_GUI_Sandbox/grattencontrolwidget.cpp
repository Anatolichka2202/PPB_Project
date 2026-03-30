#include "grattencontrolwidget.h"
#include "ui_grattencontrolwidget.h"
#include <QDateTime>
#include <QDebug>

GrattenControlWidget::GrattenControlWidget(IAkipController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GrattenControlWidget)
    , m_controller(controller)
{
    ui->setupUi(this);

    // Настройка комбобокса единиц амплитуды (только dBm)
    ui->cmbAmplUnit->clear();
    ui->cmbAmplUnit->addItem("dBm");

    // Подключение кнопок управления
    connect(ui->btnSetFreq, &QPushButton::clicked, this, &GrattenControlWidget::onSetFreqClicked);
    connect(ui->btnQueryFreq, &QPushButton::clicked, this, &GrattenControlWidget::onQueryFreqClicked);
    connect(ui->btnSetAmpl, &QPushButton::clicked, this, &GrattenControlWidget::onSetAmplClicked);
    connect(ui->btnQueryAmpl, &QPushButton::clicked, this, &GrattenControlWidget::onQueryAmplClicked);
    connect(ui->btnQueryOutput, &QPushButton::clicked, this, &GrattenControlWidget::onQueryOutputClicked);
    connect (ui->botton_onof, &QPushButton::clicked, this, &GrattenControlWidget::on_botton_onof_clicked);
    connect(ui->btnSend, &QPushButton::clicked, this, &GrattenControlWidget::onSendCommandClicked);
    connect(ui->cmdLineEdit, &QLineEdit::returnPressed, this, &GrattenControlWidget::onSendCommandClicked);

    // Подключение сигналов контроллера
    connect(m_controller, &IAkipController::frequencyChanged, this, &GrattenControlWidget::onFrequencyChanged);
    connect(m_controller, &IAkipController::amplitudeChanged, this, &GrattenControlWidget::onAmplitudeChanged);
    connect(m_controller, &IAkipController::outputChanged, this, &GrattenControlWidget::onOutputChanged);
    connect(m_controller, &IAkipController::errorOccurred, this, &GrattenControlWidget::onError);

    // Настройка терминала
    ui->terminalEdit->setReadOnly(true);
    ui->terminalEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont font("Courier New", 9);
    ui->terminalEdit->setFont(font);
    ui->cmdLineEdit->setFont(font);

    m_controller->queryOutput(1);
    ui->out_label->setStyleSheet("background-color: red; border-radius: 10px;");
    appendToTerminal("Терминал готов. Введите SCPI команду.");
}

GrattenControlWidget::~GrattenControlWidget()
{
    delete ui;
}

void GrattenControlWidget::appendToTerminal(const QString &text, bool isCommand, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString prefix;
    if (isCommand)
        prefix = "▶ ";
    else if (isError)
        prefix = "⚠ ";
    else
        prefix = "✓ ";

    QString formatted = QString("[%1] %2%3").arg(timestamp).arg(prefix).arg(text);
    ui->terminalEdit->appendPlainText(formatted);
    qDebug() << "[Terminal]" << formatted;
    emit logMessage(formatted); // для дублирования в главный лог (если нужно)
}

void GrattenControlWidget::onSendCommandClicked()
{
    QString cmd = ui->cmdLineEdit->text().trimmed();
    if (cmd.isEmpty())
        return;

    appendToTerminal(cmd, true); // показать введённую команду
    ui->cmdLineEdit->clear();

    m_timer.start();
    if (cmd.contains('?')) {
        QString response = m_controller->queryCommand(cmd);
        qint64 elapsed = m_timer.elapsed();
        if (!response.isEmpty()) {
            appendToTerminal(QString("Ответ (%1 мс): %2").arg(elapsed).arg(response));
        } else {
            appendToTerminal("Ошибка: пустой ответ или таймаут", false, true);
        }
    } else {
        bool ok = m_controller->sendCommand(cmd);
        qint64 elapsed = m_timer.elapsed();
        if (ok) {
            appendToTerminal(QString("Команда отправлена (%1 мс)").arg(elapsed));
        } else {
            appendToTerminal("Ошибка отправки команды", false, true);
        }
    }
}

void GrattenControlWidget::onSetFreqClicked()
{
    bool ok;
    double freq = ui->editFreq->text().toDouble(&ok);
    if (!ok || freq <= 0) {
        appendToTerminal("Некорректная частота", false, true);
        return;
    }
    m_timer.start();
    bool result = m_controller->setFrequency(1, freq);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToTerminal(QString("Частота установлена в %1 Гц за %2 мс").arg(freq).arg(elapsed));
    } else {
        appendToTerminal("Ошибка установки частоты", false, true);
    }
}

void GrattenControlWidget::onQueryFreqClicked()
{
    m_timer.start();
    double freq = m_controller->queryFrequency(1);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->editFreq->setText(QString::number(freq));
        updateLastOpTime(elapsed);
        appendToTerminal(QString("Частота = %1 Гц (время %2 мс)").arg(freq).arg(elapsed));
    } else {
        appendToTerminal("Ошибка запроса частоты", false, true);
    }
}

void GrattenControlWidget::onSetAmplClicked()
{
    bool ok;
    double ampl = ui->editAmpl->text().toDouble(&ok);
    if (!ok) {
        appendToTerminal("Некорректная амплитуда (не число)", false, true);
        return;
    }
    QString unit = ui->cmbAmplUnit->currentText(); // всегда dBm
    m_timer.start();
    bool result = m_controller->setAmplitude(1, ampl, unit);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToTerminal(QString("Амплитуда установлена в %1 %2 за %3 мс").arg(ampl).arg(unit).arg(elapsed));
    } else {
        appendToTerminal("Ошибка установки амплитуды", false, true);
    }
}

void GrattenControlWidget::onQueryAmplClicked()
{
    m_timer.start();
    double ampl = m_controller->queryAmplitude(1);
    qint64 elapsed = m_timer.elapsed();
    if (ampl > -999) { // допустимый диапазон включает отрицательные значения
        ui->editAmpl->setText(QString::number(ampl));
        updateLastOpTime(elapsed);
        appendToTerminal(QString("Амплитуда = %1 dBm (время %2 мс)").arg(ampl).arg(elapsed));
    } else {
        appendToTerminal("Ошибка запроса амплитуды", false, true);
    }
}



void GrattenControlWidget::onQueryOutputClicked()
{
    m_timer.start();
    bool on = m_controller->queryOutput(1);
    qint64 elapsed = m_timer.elapsed();
    updateOutputUI(on);
    updateLastOpTime(elapsed);
    appendToTerminal(QString("Выход = %1 (время %2 мс)").arg(on ? "вкл" : "выкл").arg(elapsed));
}

void GrattenControlWidget::onFrequencyChanged(int channel, double freq)
{
    if (channel == 1) {
        ui->editFreq->setText(QString::number(freq));
        appendToTerminal(QString("Событие: частота канала %1 изменена на %2 Гц").arg(channel).arg(freq));
    }
}

void GrattenControlWidget::onAmplitudeChanged(int channel, double amplitude)
{
    if (channel == 1) {
        ui->editAmpl->setText(QString::number(amplitude));
        appendToTerminal(QString("Событие: амплитуда канала %1 изменена на %2 dBm").arg(channel).arg(amplitude));
    }
}

void GrattenControlWidget::onOutputChanged(int channel, bool enabled)
{
    if (channel == 1) {
        updateOutputUI(enabled);
        appendToTerminal(QString("Событие: выход канала %1 %2")
                             .arg(channel).arg(enabled ? "включён" : "выключен"));
    }
}

void GrattenControlWidget::onError(const QString &error)
{
    appendToTerminal("ОШИБКА: " + error, false, true);
}

void GrattenControlWidget::updateLastOpTime(qint64 elapsedMs)
{
    ui->lblLastOpTime->setText(QString("Время последней операции: %1 мс").arg(elapsedMs));
}


void GrattenControlWidget::on_botton_onof_clicked()
{

    bool newState = ui->botton_onof->isChecked();

    m_timer.start();
    bool result = m_controller->setOutput(1, newState);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToTerminal(QString("Команда на %1 выхода отправлена (%2 мс)")
                             .arg(newState ? "включение" : "выключение").arg(elapsed));
        // Ждём сигнала outputChanged, который обновит UI
    } else {
        appendToTerminal("Ошибка отправки команды на выход", false, true);
        // Возвращаем кнопку в исходное состояние (противоположное новому)
        ui->botton_onof->blockSignals(true);
        ui->botton_onof->setChecked(!newState);
        ui->botton_onof->blockSignals(false);
    }
}

void GrattenControlWidget::updateOutputUI(bool isOn)
{
    // Обновляем кнопку (блокируем сигналы, чтобы не вызвать рекурсию)
    ui->botton_onof->blockSignals(true);
    ui->botton_onof->setChecked(isOn);
    ui->botton_onof->blockSignals(false);

    // Обновляем текст кнопки (опционально)
    ui->botton_onof->setText(isOn ? "Выключить" : "Включить");

    // Обновляем лампочку
    QString color = isOn ? "green" : "red";
    ui->out_label->setStyleSheet(QString("background-color: %1; border-radius: 10px;").arg(color));
}
