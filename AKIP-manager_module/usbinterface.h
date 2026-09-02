#ifndef USBINTERFACE_H
#define USBINTERFACE_H

#include <QObject>
#include <QByteArray>
#include <QString>

// Определяем, под какой ОС собираем
#ifdef Q_OS_WIN
#include <QLibrary>

// Windows: подключаем windows.h и объявления CH375DLL.
// Сама DLL загружается динамически в runtime, поэтому import .lib для сборки
// приложения больше не требуется.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Временно определяем min/max, чтобы CH375DLL_EN.H не делал этого сам
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#include "ch375_sdk/CH375DLL_EN.H"

#undef min
#undef max

// Тип дескриптора – HANDLE
typedef HANDLE DeviceHandle;
const DeviceHandle InvalidDeviceHandle = INVALID_HANDLE_VALUE;
#else
// Linux: подключаем SDK от производителя
#include "ch37x_lib.h"

typedef int DeviceHandle;
const DeviceHandle InvalidDeviceHandle = -1;
#endif

class UsbInterface : public QObject
{
    Q_OBJECT

public:
    explicit UsbInterface(QObject *parent = nullptr);
    ~UsbInterface();

    bool open(int index = 0);
    void close();
    bool isOpen() const;

    bool sendScpiCommand(const QString &command);
    QString queryScpiCommand(const QString &command, int timeoutMs = 1000);

    bool setWriteTimeout(int ms);
    bool setReadTimeout(int ms);

    bool setOutput(bool enable, int channel = 1);
    bool setFrequency(double freqHz, int channel = 1);
    bool setAmplitude(double amplitude, const QString &unit = "VPP", int channel = 1);
    bool setWaveform(const QString &waveform, int channel = 1);
    QString getIdentity();
    bool resetDevice();

signals:
    void deviceOpened();
    void deviceClosed();
    void errorOccurred(const QString &error);

private:
    DeviceHandle m_deviceHandle;
    bool m_isOpen;
    int m_writeTimeout;
    int m_readTimeout;

#ifdef Q_OS_WIN
    using CH375OpenDeviceFn = decltype(&CH375OpenDevice);
    using CH375CloseDeviceFn = decltype(&CH375CloseDevice);
    using CH375SetTimeoutFn = decltype(&CH375SetTimeout);
    using CH375WriteDataFn = decltype(&CH375WriteData);
    using CH375ReadDataFn = decltype(&CH375ReadData);

    bool ensureWindowsApiLoaded();
    void clearWindowsApi();
    ULONG handleToULong() const;

    QLibrary m_ch375Library;
    CH375OpenDeviceFn m_ch375OpenDevice = nullptr;
    CH375CloseDeviceFn m_ch375CloseDevice = nullptr;
    CH375SetTimeoutFn m_ch375SetTimeout = nullptr;
    CH375WriteDataFn m_ch375WriteData = nullptr;
    CH375ReadDataFn m_ch375ReadData = nullptr;
#endif

    QString waitForResponse(int timeoutMs);
};

#endif // USBINTERFACE_H
