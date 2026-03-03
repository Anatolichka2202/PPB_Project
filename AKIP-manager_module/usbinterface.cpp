#include "usbinterface.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

// Для Linux добавляем необходимые заголовки
#ifdef Q_OS_LINUX
#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif

UsbInterface::UsbInterface(QObject *parent)
    : QObject(parent)
    , m_deviceHandle(InvalidDeviceHandle)
    , m_isOpen(false)
    , m_writeTimeout(2000)
    , m_readTimeout(2000)
{
}

UsbInterface::~UsbInterface()
{
    close();
}

#ifdef Q_OS_WIN
ULONG UsbInterface::handleToULong() const
{
    return static_cast<ULONG>(reinterpret_cast<ULONG_PTR>(m_deviceHandle));
}
#endif

bool UsbInterface::open(int index)
{
    if (m_isOpen) {
        close();
    }

#ifdef Q_OS_WIN
    // Windows implementation
    m_deviceHandle = CH375OpenDevice(index);
    if (m_deviceHandle == InvalidDeviceHandle) {
        emit errorOccurred("Не удалось открыть устройство. Проверьте подключение и драйвер.");
        return false;
    }

    // Устанавливаем таймауты
    CH375SetTimeout(handleToULong(), m_writeTimeout, m_readTimeout);

#else
    // Linux implementation
    char devname[20];
    snprintf(devname, sizeof(devname), "/dev/ch37x%d", index);
    m_deviceHandle = CH37XOpenDevice(devname, true); // non-block
    if (m_deviceHandle < 0) {
        emit errorOccurred(QString("Не удалось открыть устройство %1: %2")
                               .arg(devname).arg(strerror(errno)));
        return false;
    }

    // Получаем информацию о конечных точках (обязательно)
    if (!CH37XGetDeviceEpMsg(m_deviceHandle)) {
        emit errorOccurred("Не удалось получить информацию о конечных точках");
        CH37XCloseDevice(m_deviceHandle);
        m_deviceHandle = InvalidDeviceHandle;
        return false;
    }

    CH37XSetTimeout(m_deviceHandle, m_writeTimeout, m_readTimeout);
#endif

    m_isOpen = true;
    emit deviceOpened();
    return true;
}

void UsbInterface::close()
{
    if (!m_isOpen) return;

#ifdef Q_OS_WIN
    CH375CloseDevice(handleToULong());
#else
    CH37XCloseDevice(m_deviceHandle);
#endif

    m_deviceHandle = InvalidDeviceHandle;
    m_isOpen = false;
    emit deviceClosed();
}

bool UsbInterface::isOpen() const
{
    return m_isOpen;
}

bool UsbInterface::sendScpiCommand(const QString &command)
{
    if (!m_isOpen) {
        emit errorOccurred("Устройство не открыто");
        return false;
    }

    QByteArray data = command.toLatin1() + "\n";
    ULONG length = data.size();   // имя переменной сохранено

#ifdef Q_OS_WIN
    BOOL result = CH375WriteData(handleToULong(), data.data(), &length);
    if (result && length == (ULONG)data.size()) {
        return true;
    } else {
        QString error = QString("Ошибка отправки команды. Отправлено %1 из %2 байт")
                            .arg(length).arg(data.size());
        emit errorOccurred(error);
        return false;
    }
#else
    // Linux: используем первый bulk out endpoint (можно выбрать нужный)
    uint8_t ep = CH37XGetObject(m_deviceHandle)->epmsg_bulkout.epaddr[0];
    bool ok = CH37XWriteData(m_deviceHandle, EPTYPE_BULKOUT, ep, data.data(), &length);
    if (ok && length == (ULONG)data.size()) {
        return true;
    } else {
        QString error = QString("Ошибка отправки команды. Отправлено %1 из %2 байт")
                            .arg(length).arg(data.size());
        emit errorOccurred(error);
        return false;
    }
#endif
}

QString UsbInterface::queryScpiCommand(const QString &command, int timeoutMs)
{
    if (!sendScpiCommand(command)) {
        return QString();
    }

    if (command.contains('?')) {
        return waitForResponse(timeoutMs);
    }

    return "OK";
}

QString UsbInterface::waitForResponse(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    QByteArray response;
    while (timer.elapsed() < timeoutMs) {
        char buffer[256];
        ULONG length = sizeof(buffer);   // имя сохранено

#ifdef Q_OS_WIN
        BOOL result = CH375ReadData(handleToULong(), buffer, &length);
        if (result && length > 0) {
            response.append(buffer, length);
            if (response.contains('\n') || response.contains('\r'))
                break;
        }
#else
        uint8_t ep = CH37XGetObject(m_deviceHandle)->epmsg_bulkin.epaddr[0];
        bool ok = CH37XReadData(m_deviceHandle, EPTYPE_BULKIN, ep, buffer, &length);
        if (ok && length > 0) {
            response.append(buffer, length);
            if (response.contains('\n') || response.contains('\r'))
                break;
        }
#endif
        else {
            QThread::msleep(10);
        }
    }

    response = response.trimmed();
    return QString::fromLatin1(response);
}

bool UsbInterface::setWriteTimeout(int ms)
{
    m_writeTimeout = ms;
    if (!m_isOpen) return true;

#ifdef Q_OS_WIN
    return CH375SetTimeout(handleToULong(), m_writeTimeout, m_readTimeout) != 0;
#else
    return CH37XSetTimeout(m_deviceHandle, m_writeTimeout, m_readTimeout);
#endif
}

bool UsbInterface::setReadTimeout(int ms)
{
    m_readTimeout = ms;
    if (!m_isOpen) return true;

#ifdef Q_OS_WIN
    return CH375SetTimeout(handleToULong(), m_writeTimeout, m_readTimeout) != 0;
#else
    return CH37XSetTimeout(m_deviceHandle, m_writeTimeout, m_readTimeout);
#endif
}

// Остальные методы (setOutput, setFrequency, getIdentity, resetDevice,
// setAmplitude, setWaveform) остаются без изменений, так как они
// вызывают sendScpiCommand.

bool UsbInterface::setOutput(bool enable, int channel)
{
    QString cmd = QString("OUTP:CH%1 %2").arg(channel == 1 ? "A" : "B").arg(enable ? "ON" : "OFF");
    return sendScpiCommand(cmd);
}

bool UsbInterface::setFrequency(double freqHz, int channel)
{
    QString freqStr = QString::number(freqHz, 'f', 0);
    QString cmd = QString("FREQ:CH%1 %2").arg(channel == 1 ? "A" : "B").arg(freqStr);
    return sendScpiCommand(cmd);
}

QString UsbInterface::getIdentity()
{
    return queryScpiCommand("*IDN?");
}

bool UsbInterface::resetDevice()
{
    return sendScpiCommand("*RST");
}

bool UsbInterface::setAmplitude(double amplitude, const QString &unit, int channel)
{
    QString cmd = QString("VOLT:CH%1 %2 %3")
    .arg(channel == 1 ? "A" : "B")
        .arg(amplitude)
        .arg(unit.toUpper());
    return sendScpiCommand(cmd);
}

bool UsbInterface::setWaveform(const QString &waveform, int channel)
{
    QString waveUpper = waveform.toUpper();
    QString cmd;

    if (waveUpper == "SIN" || waveUpper == "SINE") {
        cmd = QString("FUNC:CH%1 SIN").arg(channel == 1 ? "A" : "B");
    } else if (waveUpper == "SQU" || waveUpper == "SQUARE") {
        cmd = QString("FUNC:CH%1 SQUARE").arg(channel == 1 ? "A" : "B");
    } else if (waveUpper == "RAMP") {
        cmd = QString("FUNC:CH%1 RAMP").arg(channel == 1 ? "A" : "B");
    } else if (waveUpper == "PULSE") {
        cmd = QString("FUNC:CH%1 PULSE").arg(channel == 1 ? "A" : "B");
    } else {
        cmd = QString("FUNC:CH%1 %2").arg(channel == 1 ? "A" : "B").arg(waveform);
    }

    return sendScpiCommand(cmd);
}
