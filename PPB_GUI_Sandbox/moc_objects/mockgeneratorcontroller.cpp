#include "mockgeneratorcontroller.h"
#include <QDebug>
#include <QTimer>
MockGeneratorController::MockGeneratorController(QObject *parent)
    : IAkipController(parent)
    , m_isOpen(false)
    , m_available(false)
    , m_freq(158000000.0)     // 1 МГц
    , m_output(false)
    , m_ampl(0.0)           // 0 dBm
    , m_amFreq(1000.0)
    , m_amDepth(50.0)
    , m_amState(false)
{
}

MockGeneratorController::~MockGeneratorController()
{
    closeDevice();
}

bool MockGeneratorController::openDevice(int)
{
    m_isOpen = true;
    m_available = true;
    emit opened();
    emit availabilityChanged(true);

    // Эмулируем задержку перед отправкой начального состояния
    // (чтобы UI показал "ожидание")
    QTimer::singleShot(100, this, [this]() {
        emit frequencyChanged(1, m_freq);
        emit amplitudeChanged(1, m_ampl);
        emit outputChanged(1, m_output);
        emit amFrequencyChanged(1, m_amFreq);
        emit amDepthChanged(1, m_amDepth);
        emit amStateChanged(1, m_amState);
    });
    return true;
}

void MockGeneratorController::closeDevice()
{
    m_isOpen = false;
    m_available = false;
    emit closed();
    emit availabilityChanged(false);
}

bool MockGeneratorController::isOpen() const
{
    return m_isOpen;
}

bool MockGeneratorController::isAvailable() const
{
    return m_available && m_isOpen;
}

bool MockGeneratorController::setFrequency(int channel, double freqHz)
{
    if (!isAvailable()) return false;
    m_freq = freqHz;
    // Эмулируем задержку подтверждения
    QTimer::singleShot(50, this, [this, channel, freqHz]() {
        emit frequencyChanged(channel, freqHz);
    });
    return true;
}

bool MockGeneratorController::setOutput(int channel, bool enable)
{
    if (!isAvailable()) return false;
    m_output = enable;
    // Имитация задержки ответа (можно сделать случайной для теста)
    QTimer::singleShot(100, this, [this, channel, enable]() {
        emit outputChanged(channel, enable);
    });
    return true;
}

bool MockGeneratorController::setAmplitude(int channel, double amplitude, const QString &unit)
{
    Q_UNUSED(unit);
    if (!isAvailable()) return false;
    m_ampl = amplitude;
    QTimer::singleShot(50, this, [this, channel, amplitude]() {
        emit amplitudeChanged(channel, amplitude);
    });
    return true;
}

bool MockGeneratorController::setWaveform(int, const QString&)
{
    return true;
}

bool MockGeneratorController::setDutyCycle(int, double)
{
    return true;
}

bool MockGeneratorController::setAMFrequency(int channel, double freqHz)
{
    if (!isAvailable()) return false;
    m_amFreq = freqHz;
    QTimer::singleShot(50, this, [this, channel, freqHz]() {
        emit amFrequencyChanged(channel, freqHz);
    });
    return true;
}

bool MockGeneratorController::setAMDepth(int channel, double percent)
{
    if (!isAvailable()) return false;
    m_amDepth = percent;
    QTimer::singleShot(50, this, [this, channel, percent]() {
        emit amDepthChanged(channel, percent);
    });
    return true;
}

bool MockGeneratorController::setAMState(int channel, bool enable)
{
    if (!isAvailable()) return false;
    m_amState = enable;
    QTimer::singleShot(50, this, [this, channel, enable]() {
        emit amStateChanged(channel, enable);
    });
    return true;
}

QString MockGeneratorController::getIdentity()
{
    return "Mock Generator v1.0";
}

bool MockGeneratorController::reset()
{
    m_freq = 1000000.0;
    m_output = false;
    m_ampl = 0.0;
    m_amFreq = 1000.0;
    m_amDepth = 50.0;
    m_amState = false;
    // Эмитим изменения
    emit frequencyChanged(1, m_freq);
    emit outputChanged(1, m_output);
    emit amplitudeChanged(1, m_ampl);
    emit amFrequencyChanged(1, m_amFreq);
    emit amDepthChanged(1, m_amDepth);
    emit amStateChanged(1, m_amState);
    return true;
}

double MockGeneratorController::queryFrequency(int)
{
    return m_freq;
}

bool MockGeneratorController::queryOutput(int)
{
    return m_output;
}

double MockGeneratorController::queryAmplitude(int)
{
    return m_ampl;
}

QString MockGeneratorController::queryWaveform(int)
{
    return "SINE";
}

double MockGeneratorController::queryDutyCycle(int)
{
    return 50.0;
}

double MockGeneratorController::queryAMFrequency(int)
{
    return m_amFreq;
}

double MockGeneratorController::queryAMDepth(int)
{
    return m_amDepth;
}

bool MockGeneratorController::queryAMState(int)
{
    return m_amState;
}

bool MockGeneratorController::sendCommand(const QString &cmd)
{
    // Эмуляция простых команд, например, ":OUTPut:STATE ON" и т.п.
    if (cmd.startsWith(":OUTPut:STATE", Qt::CaseInsensitive)) {
        bool on = cmd.contains("ON", Qt::CaseInsensitive);
        setOutput(1, on);
        return true;
    }
    // Добавьте обработку других команд по желанию
    return true;
}

QString MockGeneratorController::queryCommand(const QString &cmd)
{
    if (cmd == ":OUTPut:STATE?") {
        return m_output ? "ON" : "OFF";
    }
    if (cmd == ":FREQuency:CW?") {
        return QString::number(m_freq);
    }
    if (cmd == ":POWer:LEVEL?") {
        return QString::number(m_ampl);
    }
    return "";
}

QStringList MockGeneratorController::availableCommands() const
{
    return { "*IDN?", ":FREQ:CW?", ":OUTP:STATE?" };
}

bool MockGeneratorController::sendCommandTimed(const QString &cmd, qint64 &elapsedMs)
{
    elapsedMs = 10; // симуляция времени
    return sendCommand(cmd);
}

bool MockGeneratorController::queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs)
{
    elapsedMs = 10;
    response = queryCommand(cmd);
    return !response.isEmpty();
}
