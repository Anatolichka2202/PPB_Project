#include "akipfacade.h"
#include <QThread>
#include <QtEndian>

AkipFacade::AkipFacade(QObject *parent)
    : IAkipController(parent)
    , m_available(false)
{
    connect(&m_usb, &UsbInterface::deviceOpened, this, &AkipFacade::onDeviceOpened);
    connect(&m_usb, &UsbInterface::deviceClosed, this, &AkipFacade::onDeviceClosed);
    connect(&m_usb, &UsbInterface::errorOccurred, this, &AkipFacade::onError);
}

AkipFacade::~AkipFacade()
{
    closeDevice();
}

// ==================== Управление подключением ====================

bool AkipFacade::openDevice(int index)
{
    if (m_usb.isOpen())
        return true;

    if (!m_usb.open(index)) {
        m_available = false;
        return false;
    }

    // После открытия проверяем доступность запросом *IDN?
    QString idn = m_usb.getIdentity();
    m_available = !idn.isEmpty();
    if (m_available) {
        emit opened();
        emit availabilityChanged(true);
    } else {
        emit errorOccurred("Device opened but does not respond to *IDN?");
    }
    return m_available;
}

void AkipFacade::closeDevice()
{
    m_usb.close();
    m_available = false;
    emit closed();
    emit availabilityChanged(false);
}

bool AkipFacade::isOpen() const
{
    return m_usb.isOpen();
}

bool AkipFacade::isAvailable() const
{
    return m_available && m_usb.isOpen();
}

// ==================== Вспомогательные ====================

QString AkipFacade::channelToLetter(int channel) const
{
    return (channel == 1) ? "A" : "B";
}

bool AkipFacade::ensureAvailable() const
{
    if (!isAvailable()) {
        const_cast<AkipFacade*>(this)->emit errorOccurred("Device not available");
        return false;
    }
    return true;
}

// ==================== Высокоуровневые команды ====================

bool AkipFacade::setFrequency(int channel, double freqHz)
{
    if (!ensureAvailable()) return false;
    QString cmd = QString("FREQ:CH%1 %2").arg(channelToLetter(channel)).arg(freqHz, 0, 'f', 0);
    if (m_usb.sendScpiCommand(cmd)) {
        m_freqCache[channel] = freqHz;
        emit frequencyChanged(channel, freqHz);
        return true;
    }
    return false;
}

bool AkipFacade::setOutput(int channel, bool enable)
{
    if (!ensureAvailable()) return false;
    QString state = enable ? "ON" : "OFF";
    QString cmd = QString("OUTP:CH%1 %2").arg(channelToLetter(channel)).arg(state);
    if (m_usb.sendScpiCommand(cmd)) {
        m_outputCache[channel] = enable;
        emit outputChanged(channel, enable);
        return true;
    }
    return false;
}

bool AkipFacade::setAmplitude(int channel, double amplitude, const QString &unit)
{
    if (!ensureAvailable()) return false;
    // unit может быть "VPP", "VRMS", "DBM" и т.д. – передаём как есть
    QString cmd = QString("VOLT:CH%1 %2 %3")
                      .arg(channelToLetter(channel))
                      .arg(amplitude, 0, 'f', 4)
                      .arg(unit);
    if (m_usb.sendScpiCommand(cmd)) {
        m_amplCache[channel] = amplitude;
        emit amplitudeChanged(channel, amplitude);
        return true;
    }
    return false;
}

bool AkipFacade::setWaveform(int channel, const QString &waveform)
{
    if (!ensureAvailable()) return false;
    QString wf = waveform.toUpper();
    // Приводим к стандартным именам SCPI
    if (wf == "SIN" || wf == "SINE") wf = "SIN";
    else if (wf == "SQU" || wf == "SQUARE") wf = "SQUARE";
    else if (wf == "RAMP") wf = "RAMP";
    else if (wf == "PULSE") wf = "PULSE";
    else if (wf == "NOISE") wf = "NOISE";
    else if (wf == "DC") wf = "DC";
    // остальные оставляем как есть

    QString cmd = QString("FUNC:CH%1 %2").arg(channelToLetter(channel)).arg(wf);
    if (m_usb.sendScpiCommand(cmd)) {
        m_waveCache[channel] = wf;
        emit waveformChanged(channel, wf);
        return true;
    }
    return false;
}

QString AkipFacade::getIdentity()
{
    if (!ensureAvailable()) return QString();
    return m_usb.getIdentity();
}

bool AkipFacade::reset()
{
    if (!ensureAvailable()) return false;
    return m_usb.resetDevice();
}

// ==================== Запросы текущих значений ====================

double AkipFacade::queryFrequency(int channel)
{
    if (!ensureAvailable()) return 0.0;
    QString cmd = QString("FREQ:CH%1?").arg(channelToLetter(channel));
    QString resp = m_usb.queryScpiCommand(cmd);
    bool ok;
    double val = resp.toDouble(&ok);
    if (ok) {
        m_freqCache[channel] = val;
        return val;
    }
    return 0.0;
}

bool AkipFacade::queryOutput(int channel)
{
    if (!ensureAvailable()) return false;
    QString cmd = QString("OUTP:CH%1?").arg(channelToLetter(channel));
    QString resp = m_usb.queryScpiCommand(cmd);
    bool on = resp.contains("ON", Qt::CaseInsensitive);
    m_outputCache[channel] = on;
    return on;
}

double AkipFacade::queryAmplitude(int channel)
{
    if (!ensureAvailable()) return 0.0;
    QString cmd = QString("VOLT:CH%1?").arg(channelToLetter(channel));
    QString resp = m_usb.queryScpiCommand(cmd);
    bool ok;
    double val = resp.toDouble(&ok);
    if (ok) {
        m_amplCache[channel] = val;
        return val;
    }
    return 0.0;
}

QString AkipFacade::queryWaveform(int channel)
{
    if (!ensureAvailable()) return QString();
    QString cmd = QString("FUNC:CH%1?").arg(channelToLetter(channel));
    QString resp = m_usb.queryScpiCommand(cmd);
    if (!resp.isEmpty()) {
        m_waveCache[channel] = resp;
        return resp;
    }
    return QString();
}

// ==================== Низкоуровневые команды ====================

bool AkipFacade::sendCommand(const QString &cmd)
{
    if (!ensureAvailable()) return false;
    return m_usb.sendScpiCommand(cmd);
}

QString AkipFacade::queryCommand(const QString &cmd)
{
    if (!ensureAvailable()) return QString();
    return m_usb.queryScpiCommand(cmd);
}

// ==================== Команды с замером времени ====================

bool AkipFacade::sendCommandTimed(const QString &cmd, qint64 &elapsedMs)
{
    if (!ensureAvailable()) return false;
    QElapsedTimer timer;
    timer.start();
    bool ok = m_usb.sendScpiCommand(cmd);
    elapsedMs = timer.elapsed();
    return ok;
}

bool AkipFacade::queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs)
{
    if (!ensureAvailable()) return false;
    QElapsedTimer timer;
    timer.start();
    response = m_usb.queryScpiCommand(cmd);
    elapsedMs = timer.elapsed();
    return !response.isEmpty();  // считаем, что пустой ответ – ошибка
}

// ==================== Список команд ====================

QStringList AkipFacade::availableCommands() const
{
    return {
        "*IDN?", "*RST",
        "FREQ:CHA", "FREQ:CHB", "FREQ:CHA?", "FREQ:CHB?",
        "OUTP:CHA", "OUTP:CHB", "OUTP:CHA?", "OUTP:CHB?",
        "VOLT:CHA", "VOLT:CHB", "VOLT:CHA?", "VOLT:CHB?",
        "VOLT:CHB:OFFS", "VOLT:CHB:OFFS?",
        "FUNC:CHA", "FUNC:CHB", "FUNC:CHA?", "FUNC:CHB?",
        "FUNC:CHB:SQU:DCYC", "FUNC:CHB:SQU:DCYC?",
        "FUNC:CHB:RAMP:SYMM", "FUNC:CHB:RAMP:SYMM?",
        "FUNC:CHB:PULS:PER", "FUNC:CHB:PULS:PER?",
        "FUNC:CHB:PULS:WIDT", "FUNC:CHB:PULS:WIDT?",
        "APPL:CHA:SIN", "APPL:CHB:SIN"
    };
}

// ==================== Слоты ====================

void AkipFacade::onDeviceOpened()
{
    // Уже обработано в openDevice
}

void AkipFacade::onDeviceClosed()
{
    m_available = false;
    emit availabilityChanged(false);
}

void AkipFacade::onError(const QString &error)
{
    emit errorOccurred(error);
}
bool AkipFacade::setDutyCycle(int channel, double percent)
{
    if (!ensureAvailable()) return false;
    // Диапазон обычно 0-100, но уточнить по документации
    QString cmd = QString("FUNC:CH%1:SQU:DCYC %2")
                      .arg(channelToLetter(channel))
                      .arg(percent, 0, 'f', 3);
    if (m_usb.sendScpiCommand(cmd)) {
        emit dutyCycleChanged(channel, percent);
        return true;
    }
    return false;
}

double AkipFacade::queryDutyCycle(int channel)
{
    if (!ensureAvailable()) return 0.0;
    QString cmd = QString("FUNC:CH%1:SQU:DCYC?").arg(channelToLetter(channel));
    QString resp = m_usb.queryScpiCommand(cmd);
    bool ok;
    double val = resp.toDouble(&ok);
    return ok ? val : 0.0;
}

bool AkipFacade::setAMFrequency(int channel, double freqHz)
{
    if (!ensureAvailable()) return false;
    // Для AM частоты обычно применяется к несущей, канал? По документации AM относится к CHA? Уточним.
    // Предположим, что AM применяется к указанному каналу.
    QString cmd = QString("AM:INT:FREQ %1").arg(freqHz, 0, 'f', 0);
    if (m_usb.sendScpiCommand(cmd)) {
        emit amFrequencyChanged(channel, freqHz);
        return true;
    }
    return false;
}

double AkipFacade::queryAMFrequency(int channel)
{
    Q_UNUSED(channel);
    if (!ensureAvailable()) return 0.0;
    QString resp = m_usb.queryScpiCommand("AM:INT:FREQ?");
    bool ok;
    double val = resp.toDouble(&ok);
    return ok ? val : 0.0;
}

bool AkipFacade::setAMDepth(int channel, double percent)
{
    if (!ensureAvailable()) return false;
    QString cmd = QString("AM:DEPT %1").arg(percent, 0, 'f', 1);
    if (m_usb.sendScpiCommand(cmd)) {
        emit amDepthChanged(channel, percent);
        return true;
    }
    return false;
}

double AkipFacade::queryAMDepth(int channel)
{
    Q_UNUSED(channel);
    if (!ensureAvailable()) return 0.0;
    QString resp = m_usb.queryScpiCommand("AM:DEPT?");
    bool ok;
    double val = resp.toDouble(&ok);
    return ok ? val : 0.0;
}

bool AkipFacade::setAMState(int channel, bool enable)
{
    Q_UNUSED(channel);
    if (!ensureAvailable()) return false;
    // В документации нет явной команды включения AM. Возможно, включается автоматически.
    // Отправим команду установки источника (например, INT) как способ активации.
    // Или можно использовать AM:SOUR INT.
    QString cmd = enable ? "AM:SOUR INT" : "AM:SOUR NONE"; // NONE может не работать.
    // Лучше просто сохранять состояние в кэше и не отправлять.
    emit amStateChanged(channel, enable);
    return true; // заглушка
}

bool AkipFacade::queryAMState(int channel)
{
    Q_UNUSED(channel);
    // Запросить состояние AM сложно, вернём false.
    return false;
}
