#include "usbinterface.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <cstdint>

#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#endif

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
#ifdef Q_OS_WIN
    clearWindowsApi();
#endif
}

#ifdef Q_OS_WIN
ULONG UsbInterface::handleToULong() const
{
    // Сохраняем существующую семантику WCH API, использованную проектом:
    // после CH375OpenDevice дескриптор передаётся в функции с ULONG-параметром.
    return static_cast<ULONG>(reinterpret_cast<ULONG_PTR>(m_deviceHandle));
}

void UsbInterface::clearWindowsApi()
{
    m_ch375OpenDevice = nullptr;
    m_ch375CloseDevice = nullptr;
    m_ch375SetTimeout = nullptr;
    m_ch375WriteData = nullptr;
    m_ch375ReadData = nullptr;

    if (m_ch375Library.isLoaded()) {
        m_ch375Library.unload();
    }
}

bool UsbInterface::ensureWindowsApiLoaded()
{
    if (m_ch375Library.isLoaded()
        && m_ch375OpenDevice
        && m_ch375CloseDevice
        && m_ch375SetTimeout
        && m_ch375WriteData
        && m_ch375ReadData) {
        return true;
    }

    const QString applicationDll =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("CH375DLL64.dll"));

    // Сначала DLL рядом с PPB.exe, затем стандартный Windows DLL search path.
    const QStringList candidates = {
        applicationDll,
        QStringLiteral("CH375DLL64"),
        QStringLiteral("CH375DLL")
    };

    QStringList loadErrors;

    for (const QString &candidate : candidates) {
        clearWindowsApi();
        m_ch375Library.setFileName(candidate);

        if (!m_ch375Library.load()) {
            loadErrors.append(QStringLiteral("%1: %2").arg(candidate, m_ch375Library.errorString()));
            continue;
        }

        m_ch375OpenDevice = reinterpret_cast<CH375OpenDeviceFn>(
            m_ch375Library.resolve("CH375OpenDevice"));
        m_ch375CloseDevice = reinterpret_cast<CH375CloseDeviceFn>(
            m_ch375Library.resolve("CH375CloseDevice"));
        m_ch375SetTimeout = reinterpret_cast<CH375SetTimeoutFn>(
            m_ch375Library.resolve("CH375SetTimeout"));
        m_ch375WriteData = reinterpret_cast<CH375WriteDataFn>(
            m_ch375Library.resolve("CH375WriteData"));
        m_ch375ReadData = reinterpret_cast<CH375ReadDataFn>(
            m_ch375Library.resolve("CH375ReadData"));

        if (m_ch375OpenDevice
            && m_ch375CloseDevice
            && m_ch375SetTimeout
            && m_ch375WriteData
            && m_ch375ReadData) {
            qInfo() << "Loaded CH375 runtime from" << m_ch375Library.fileName();
            return true;
        }

        loadErrors.append(QStringLiteral("%1: DLL loaded, but required CH375 exports are missing")
                              .arg(candidate));
    }

    clearWindowsApi();
    emit errorOccurred(
        QStringLiteral(
            "Не найден runtime CH375DLL64.dll для USB-интерфейса АКИП. "
            "Установите официальный драйвер/runtime WCH или поместите CH375DLL64.dll рядом с PPB.exe.\n%1")
            .arg(loadErrors.join(QLatin1Char('\n'))));
    return false;
}
#endif

bool UsbInterface::open(int index)
{
    if (m_isOpen) {
        close();
    }

#ifdef Q_OS_WIN
    if (!ensureWindowsApiLoaded()) {
        return false;
    }

    m_deviceHandle = m_ch375OpenDevice(static_cast<ULONG>(index));
    if (m_deviceHandle == InvalidDeviceHandle) {
        emit errorOccurred("Не удалось открыть устройство. Проверьте подключение и драйвер.");
        return false;
    }

    m_ch375SetTimeout(handleToULong(),
                      static_cast<ULONG>(m_writeTimeout),
                      static_cast<ULONG>(m_readTimeout));

#else
    char devname[20];
    snprintf(devname, sizeof(devname), "/dev/ch37x%d", index);
    m_deviceHandle = CH37XOpenDevice(devname, true); // non-block
    if (m_deviceHandle < 0) {
        emit errorOccurred(QString("Не удалось открыть устройство %1: %2")
                               .arg(devname).arg(strerror(errno)));
        return false;
    }

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
    if (m_ch375CloseDevice) {
        m_ch375CloseDevice(handleToULong());
    }
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

#ifdef Q_OS_WIN
    ULONG length = static_cast<ULONG>(data.size());
    BOOL result = m_ch375WriteData(handleToULong(), data.data(), &length);
    if (result && length == static_cast<ULONG>(data.size())) {
        return true;
    } else {
        QString error = QString("Ошибка отправки команды. Отправлено %1 из %2 байт")
                            .arg(length).arg(data.size());
        emit errorOccurred(error);
        return false;
    }
#else
    uint32_t length = data.size();
    uint8_t ep = CH37XGetObject(m_deviceHandle)->epmsg_bulkout.epaddr[0];
    bool ok = CH37XWriteData(m_deviceHandle, EPTYPE_BULKOUT, ep, data.data(), &length);
    if (ok && length == static_cast<uint32_t>(data.size())) {
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

#ifdef Q_OS_WIN
        ULONG length = sizeof(buffer);
        BOOL result = m_ch375ReadData(handleToULong(), buffer, &length);
        if (result && length > 0) {
            response.append(buffer, static_cast<int>(length));
            if (response.contains('\n') || response.contains('\r'))
                break;
        }
#else
        uint32_t length = sizeof(buffer);
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
    return m_ch375SetTimeout(handleToULong(),
                             static_cast<ULONG>(m_writeTimeout),
                             static_cast<ULONG>(m_readTimeout)) != 0;
#else
    return CH37XSetTimeout(m_deviceHandle, m_writeTimeout, m_readTimeout);
#endif
}

bool UsbInterface::setReadTimeout(int ms)
{
    m_readTimeout = ms;
    if (!m_isOpen) return true;

#ifdef Q_OS_WIN
    return m_ch375SetTimeout(handleToULong(),
                             static_cast<ULONG>(m_writeTimeout),
                             static_cast<ULONG>(m_readTimeout)) != 0;
#else
    return CH37XSetTimeout(m_deviceHandle, m_writeTimeout, m_readTimeout);
#endif
}

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
