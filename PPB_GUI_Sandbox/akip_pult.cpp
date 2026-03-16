#include "akip_pult.h"
#include "ui_akip_pult.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

GeneratorPult::GeneratorPult(IAkipController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GeneratorPult)
    , m_controller(controller)
    , m_currentChannel(1)
{
    ui->setupUi(this);

    if (!m_controller || !m_controller->isAvailable()) {
        appendToLog("Генератор недоступен. Пульт будет закрыт.", true);
        QTimer::singleShot(100, this, &GeneratorPult::closeIfUnavailable);
        return;
    }

    QString idn = m_controller->getIdentity();
    ui->lblIdentity->setText(idn.isEmpty() ? "Неизвестно" : idn);

    ui->cmbWaveform->addItems({"SIN", "SQUARE", "RAMP", "PULSE", "NOISE", "DC"});
    ui->cmbAmplUnit->addItems({"VPP", "VRMS", "DBM"});

    ui->radioCh1->setChecked(true);

    connect(m_controller, &IAkipController::frequencyChanged,
            this, &GeneratorPult::onFrequencyChanged);
    connect(m_controller, &IAkipController::outputChanged,
            this, &GeneratorPult::onOutputChanged);
    connect(m_controller, &IAkipController::amplitudeChanged,
            this, &GeneratorPult::onAmplitudeChanged);
    connect(m_controller, &IAkipController::waveformChanged,
            this, &GeneratorPult::onWaveformChanged);
    connect(m_controller, &IAkipController::dutyCycleChanged,
            this, &GeneratorPult::onDutyCycleChanged);
    connect(m_controller, &IAkipController::amFrequencyChanged,
            this, &GeneratorPult::onAmFrequencyChanged);
    connect(m_controller, &IAkipController::amDepthChanged,
            this, &GeneratorPult::onAmDepthChanged);
    connect(m_controller, &IAkipController::amStateChanged,
            this, &GeneratorPult::onAmStateChanged);
    connect(m_controller, &IAkipController::errorOccurred,
            this, &GeneratorPult::onError);

    connect(ui->btnSetFreq, &QPushButton::clicked,
            this, &GeneratorPult::onSetFreqClicked);
    connect(ui->btnQueryFreq, &QPushButton::clicked,
            this, &GeneratorPult::onQueryFreqClicked);
    connect(ui->btnSetOutput, &QPushButton::clicked,
            this, &GeneratorPult::onSetOutputClicked);
    connect(ui->btnQueryOutput, &QPushButton::clicked,
            this, &GeneratorPult::onQueryOutputClicked);
    connect(ui->btnSetAmpl, &QPushButton::clicked,
            this, &GeneratorPult::onSetAmplClicked);
    connect(ui->btnQueryAmpl, &QPushButton::clicked,
            this, &GeneratorPult::onQueryAmplClicked);
    connect(ui->btnSetWaveform, &QPushButton::clicked,
            this, &GeneratorPult::onSetWaveformClicked);
    connect(ui->btnQueryWaveform, &QPushButton::clicked,
            this, &GeneratorPult::onQueryWaveformClicked);
    connect(ui->btnSetDuty, &QPushButton::clicked,
            this, &GeneratorPult::onSetDutyClicked);
    connect(ui->btnQueryDuty, &QPushButton::clicked,
            this, &GeneratorPult::onQueryDutyClicked);
    connect(ui->btnSetAMFreq, &QPushButton::clicked,
            this, &GeneratorPult::onSetAMFreqClicked);
    connect(ui->btnQueryAMFreq, &QPushButton::clicked,
            this, &GeneratorPult::onQueryAMFreqClicked);
    connect(ui->btnSetAMDepth, &QPushButton::clicked,
            this, &GeneratorPult::onSetAMDepthClicked);
    connect(ui->btnQueryAMDepth, &QPushButton::clicked,
            this, &GeneratorPult::onQueryAMDepthClicked);
    connect(ui->btnSetAMState, &QPushButton::clicked,
            this, &GeneratorPult::onSetAMStateClicked);
    connect(ui->btnQueryAMState, &QPushButton::clicked,
            this, &GeneratorPult::onQueryAMStateClicked);
    connect(ui->btnReset, &QPushButton::clicked,
            this, &GeneratorPult::onResetClicked);
    connect(ui->btnSendCustom, &QPushButton::clicked,
            this, &GeneratorPult::onSendCustomClicked);
    connect(ui->customCommandEdit, &QLineEdit::returnPressed,
            this, &GeneratorPult::onSendCustomClicked);

    connect(ui->radioCh1, &QRadioButton::toggled,
            this, &GeneratorPult::onChannelChanged);
    connect(ui->radioCh2, &QRadioButton::toggled,
            this, &GeneratorPult::onChannelChanged);

    onChannelChanged();

    QTimer::singleShot(200, [this]() {
        if (m_controller && m_controller->isAvailable()) {
            onQueryFreqClicked();
            onQueryOutputClicked();
            onQueryAmplClicked();
            onQueryWaveformClicked();
            onQueryDutyClicked();
            onQueryAMFreqClicked();
            onQueryAMDepthClicked();
            onQueryAMStateClicked();
        }
    });
}

GeneratorPult::~GeneratorPult()
{
    delete ui;
}

void GeneratorPult::closeIfUnavailable()
{
    if (!m_controller || !m_controller->isAvailable()) {
        QMessageBox::critical(this, "Ошибка", "Генератор недоступен. Пульт будет закрыт.");
        close();
    }
}

void GeneratorPult::setControlsEnabled(bool enabled)
{
    ui->groupBoxChannel->setEnabled(enabled);
    ui->groupBoxFreq->setEnabled(enabled);
    ui->groupBoxOutput->setEnabled(enabled);
    ui->groupBoxAmpl->setEnabled(enabled);
    ui->groupBoxWaveform->setEnabled(enabled);
    ui->groupBoxDuty->setEnabled(enabled);
    ui->groupBoxAM->setEnabled(enabled);
    ui->groupBoxCustom->setEnabled(enabled);
    ui->btnReset->setEnabled(enabled);
}

void GeneratorPult::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_controller && !m_controller->isAvailable()) {
        closeIfUnavailable();
    }
}

void GeneratorPult::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void GeneratorPult::appendToLog(const QString &text, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString prefix = isError ? "❌" : "✓";
    QString formatted = QString("[%1] %2 %3").arg(timestamp).arg(prefix).arg(text);
    ui->logEdit->appendPlainText(formatted);
    qDebug() << "[GeneratorPult]" << formatted;
}

void GeneratorPult::updateLastOpTime(qint64 elapsedMs)
{
    ui->lblLastOpTime->setText(QString("Время: %1 мс").arg(elapsedMs));
}

bool GeneratorPult::ensureAvailable() const
{
    if (!m_controller || !m_controller->isAvailable()) {
        const_cast<GeneratorPult*>(this)->appendToLog("Генератор не доступен", true);
        return false;
    }
    return true;
}

void GeneratorPult::onChannelChanged()
{
    m_currentChannel = ui->radioCh1->isChecked() ? 1 : 2;

    QStringList cmds = m_controller->availableCommands();
    bool hasChannel2 = cmds.contains("FREQ:CHB?", Qt::CaseInsensitive) ||
                       cmds.contains("FREQuency:CH2?", Qt::CaseInsensitive);
    ui->radioCh2->setEnabled(hasChannel2);
    if (!hasChannel2 && m_currentChannel == 2) {
        ui->radioCh1->setChecked(true);
        m_currentChannel = 1;
    }

    updateChannelDependentControls();
}

void GeneratorPult::updateChannelDependentControls()
{
    // можно дополнить логику при необходимости
}

// --- Частота ---
void GeneratorPult::onSetFreqClicked()
{
    if (!ensureAvailable()) return;
    bool ok;
    double freq = ui->editFreq->text().toDouble(&ok);
    if (!ok || freq <= 0) {
        appendToLog("Некорректное значение частоты", true);
        return;
    }
    m_timer.start();
    bool result = m_controller->setFrequency(m_currentChannel, freq);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Частота канала %1 установлена в %2 Гц за %3 мс")
                        .arg(m_currentChannel).arg(freq).arg(elapsed));
    } else {
        appendToLog("Ошибка установки частоты", true);
    }
}

void GeneratorPult::onQueryFreqClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    double freq = m_controller->queryFrequency(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->editFreq->setText(QString::number(freq));
        updateLastOpTime(elapsed);
        appendToLog(QString("Частота канала %1 = %2 Гц (за %3 мс)")
                        .arg(m_currentChannel).arg(freq).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса частоты", true);
    }
}

// --- Выход ---
void GeneratorPult::onSetOutputClicked()
{
    if (!ensureAvailable()) return;
    bool enable = ui->chkOutput->isChecked();
    m_timer.start();
    bool result = m_controller->setOutput(m_currentChannel, enable);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Выход канала %1 %2 за %3 мс")
                        .arg(m_currentChannel).arg(enable ? "включён" : "выключен").arg(elapsed));
    } else {
        appendToLog("Ошибка установки выхода", true);
    }
}

void GeneratorPult::onQueryOutputClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    bool on = m_controller->queryOutput(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    ui->chkOutput->setChecked(on);
    updateLastOpTime(elapsed);
    appendToLog(QString("Выход канала %1 = %2 (за %3 мс)")
                    .arg(m_currentChannel).arg(on ? "вкл" : "выкл").arg(elapsed));
}

// --- Амплитуда ---
void GeneratorPult::onSetAmplClicked()
{
    if (!ensureAvailable()) return;
    bool ok;
    double ampl = ui->editAmpl->text().toDouble(&ok);
    if (!ok) {
        appendToLog("Некорректное значение амплитуды", true);
        return;
    }
    QString unit = ui->cmbAmplUnit->currentText();
    m_timer.start();
    bool result = m_controller->setAmplitude(m_currentChannel, ampl, unit);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Амплитуда канала %1 установлена в %2 %3 за %4 мс")
                        .arg(m_currentChannel).arg(ampl).arg(unit).arg(elapsed));
    } else {
        appendToLog("Ошибка установки амплитуды", true);
    }
}

void GeneratorPult::onQueryAmplClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    double ampl = m_controller->queryAmplitude(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (!qIsNaN(ampl)) {
        ui->editAmpl->setText(QString::number(ampl));
        updateLastOpTime(elapsed);
        appendToLog(QString("Амплитуда канала %1 = %2 (за %3 мс)")
                        .arg(m_currentChannel).arg(ampl).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса амплитуды", true);
    }
}

// --- Форма сигнала ---
void GeneratorPult::onSetWaveformClicked()
{
    if (!ensureAvailable()) return;
    QString wf = ui->cmbWaveform->currentText();
    m_timer.start();
    bool result = m_controller->setWaveform(m_currentChannel, wf);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Форма канала %1 установлена в %2 за %3 мс")
                        .arg(m_currentChannel).arg(wf).arg(elapsed));
    } else {
        appendToLog("Ошибка установки формы сигнала", true);
    }
}

void GeneratorPult::onQueryWaveformClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    QString wf = m_controller->queryWaveform(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (!wf.isEmpty()) {
        int idx = ui->cmbWaveform->findText(wf, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveform->setCurrentIndex(idx);
        updateLastOpTime(elapsed);
        appendToLog(QString("Форма канала %1 = %2 (за %3 мс)")
                        .arg(m_currentChannel).arg(wf).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса формы сигнала", true);
    }
}

// --- Duty cycle ---
void GeneratorPult::onSetDutyClicked()
{
    if (!ensureAvailable()) return;
    bool ok;
    double duty = ui->editDuty->text().toDouble(&ok);
    if (!ok || duty < 0 || duty > 100) {
        appendToLog("Некорректное значение duty cycle (0-100)", true);
        return;
    }
    m_timer.start();
    bool result = m_controller->setDutyCycle(m_currentChannel, duty);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Duty cycle канала %1 установлен в %2% за %3 мс")
                        .arg(m_currentChannel).arg(duty).arg(elapsed));
    } else {
        appendToLog("Ошибка установки duty cycle", true);
    }
}

void GeneratorPult::onQueryDutyClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    double duty = m_controller->queryDutyCycle(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (duty >= 0) {
        ui->editDuty->setText(QString::number(duty));
        updateLastOpTime(elapsed);
        appendToLog(QString("Duty cycle канала %1 = %2% (за %3 мс)")
                        .arg(m_currentChannel).arg(duty).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса duty cycle", true);
    }
}

// --- AM частота ---
void GeneratorPult::onSetAMFreqClicked()
{
    if (!ensureAvailable()) return;
    bool ok;
    double freq = ui->editAMFreq->text().toDouble(&ok);
    if (!ok || freq <= 0) {
        appendToLog("Некорректное значение AM частоты", true);
        return;
    }
    m_timer.start();
    bool result = m_controller->setAMFrequency(m_currentChannel, freq);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("AM частота канала %1 установлена в %2 Гц за %3 мс")
                        .arg(m_currentChannel).arg(freq).arg(elapsed));
    } else {
        appendToLog("Ошибка установки AM частоты", true);
    }
}

void GeneratorPult::onQueryAMFreqClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    double freq = m_controller->queryAMFrequency(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->editAMFreq->setText(QString::number(freq));
        updateLastOpTime(elapsed);
        appendToLog(QString("AM частота канала %1 = %2 Гц (за %3 мс)")
                        .arg(m_currentChannel).arg(freq).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса AM частоты", true);
    }
}

// --- AM глубина ---
void GeneratorPult::onSetAMDepthClicked()
{
    if (!ensureAvailable()) return;
    bool ok;
    double depth = ui->editAMDepth->text().toDouble(&ok);
    if (!ok || depth < 0 || depth > 100) {
        appendToLog("Некорректное значение глубины AM (0-100)", true);
        return;
    }
    m_timer.start();
    bool result = m_controller->setAMDepth(m_currentChannel, depth);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("AM глубина канала %1 установлена в %2% за %3 мс")
                        .arg(m_currentChannel).arg(depth).arg(elapsed));
    } else {
        appendToLog("Ошибка установки AM глубины", true);
    }
}

void GeneratorPult::onQueryAMDepthClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    double depth = m_controller->queryAMDepth(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    if (depth >= 0) {
        ui->editAMDepth->setText(QString::number(depth));
        updateLastOpTime(elapsed);
        appendToLog(QString("AM глубина канала %1 = %2% (за %3 мс)")
                        .arg(m_currentChannel).arg(depth).arg(elapsed));
    } else {
        appendToLog("Ошибка запроса AM глубины", true);
    }
}

// --- AM состояние ---
void GeneratorPult::onSetAMStateClicked()
{
    if (!ensureAvailable()) return;
    bool enable = ui->chkAMState->isChecked();
    m_timer.start();
    bool result = m_controller->setAMState(m_currentChannel, enable);
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("AM состояние канала %1 установлено в %2 за %3 мс")
                        .arg(m_currentChannel).arg(enable ? "вкл" : "выкл").arg(elapsed));
    } else {
        appendToLog("Ошибка установки AM состояния", true);
    }
}

void GeneratorPult::onQueryAMStateClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    bool on = m_controller->queryAMState(m_currentChannel);
    qint64 elapsed = m_timer.elapsed();
    ui->chkAMState->setChecked(on);
    updateLastOpTime(elapsed);
    appendToLog(QString("AM состояние канала %1 = %2 (за %3 мс)")
                    .arg(m_currentChannel).arg(on ? "вкл" : "выкл").arg(elapsed));
}

// --- Сброс ---
void GeneratorPult::onResetClicked()
{
    if (!ensureAvailable()) return;
    m_timer.start();
    bool result = m_controller->reset();
    qint64 elapsed = m_timer.elapsed();
    if (result) {
        updateLastOpTime(elapsed);
        appendToLog(QString("Сброс выполнен за %1 мс").arg(elapsed));
    } else {
        appendToLog("Ошибка сброса", true);
    }
}

// --- Произвольная команда ---
void GeneratorPult::onSendCustomClicked()
{
    if (!ensureAvailable()) return;
    QString cmd = ui->customCommandEdit->text().trimmed();
    if (cmd.isEmpty()) return;

    appendToLog(">> " + cmd);

    m_timer.start();
    if (cmd.contains('?')) {
        QString response = m_controller->queryCommand(cmd);
        qint64 elapsed = m_timer.elapsed();
        if (!response.isEmpty()) {
            appendToLog(QString("Ответ (%1 мс): %2").arg(elapsed).arg(response));
            updateLastOpTime(elapsed);
        } else {
            appendToLog("Пустой ответ или ошибка", true);
        }
    } else {
        bool ok = m_controller->sendCommand(cmd);
        qint64 elapsed = m_timer.elapsed();
        if (ok) {
            appendToLog(QString("Команда отправлена (%1 мс)").arg(elapsed));
            updateLastOpTime(elapsed);
        } else {
            appendToLog("Ошибка отправки команды", true);
        }
    }
}

// --- Слоты обновления от контроллера ---
void GeneratorPult::onFrequencyChanged(int channel, double freq)
{
    if (channel == m_currentChannel) {
        ui->editFreq->setText(QString::number(freq));
        appendToLog(QString("Обновление: частота канала %1 = %2 Гц").arg(channel).arg(freq));
    }
}

void GeneratorPult::onOutputChanged(int channel, bool enabled)
{
    if (channel == m_currentChannel) {
        ui->chkOutput->setChecked(enabled);
        appendToLog(QString("Обновление: выход канала %1 %2").arg(channel).arg(enabled ? "вкл" : "выкл"));
    }
}

void GeneratorPult::onAmplitudeChanged(int channel, double amplitude)
{
    if (channel == m_currentChannel) {
        ui->editAmpl->setText(QString::number(amplitude));
        appendToLog(QString("Обновление: амплитуда канала %1 = %2").arg(channel).arg(amplitude));
    }
}

void GeneratorPult::onWaveformChanged(int channel, const QString &waveform)
{
    if (channel == m_currentChannel) {
        int idx = ui->cmbWaveform->findText(waveform, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveform->setCurrentIndex(idx);
        appendToLog(QString("Обновление: форма канала %1 = %2").arg(channel).arg(waveform));
    }
}

void GeneratorPult::onDutyCycleChanged(int channel, double percent)
{
    if (channel == m_currentChannel) {
        ui->editDuty->setText(QString::number(percent));
        appendToLog(QString("Обновление: duty cycle канала %1 = %2%").arg(channel).arg(percent));
    }
}

void GeneratorPult::onAmFrequencyChanged(int channel, double freq)
{
    if (channel == m_currentChannel) {
        ui->editAMFreq->setText(QString::number(freq));
        appendToLog(QString("Обновление: AM частота канала %1 = %2 Гц").arg(channel).arg(freq));
    }
}

void GeneratorPult::onAmDepthChanged(int channel, double percent)
{
    if (channel == m_currentChannel) {
        ui->editAMDepth->setText(QString::number(percent));
        appendToLog(QString("Обновление: AM глубина канала %1 = %2%").arg(channel).arg(percent));
    }
}

void GeneratorPult::onAmStateChanged(int channel, bool enabled)
{
    if (channel == m_currentChannel) {
        ui->chkAMState->setChecked(enabled);
        appendToLog(QString("Обновление: AM состояние канала %1 = %2").arg(channel).arg(enabled ? "вкл" : "выкл"));
    }
}

void GeneratorPult::onError(const QString &error)
{
    appendToLog("ОШИБКА: " + error, true);
}
