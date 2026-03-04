#ifndef USBINTERFACE_H
#define USBINTERFACE_H

#include <QObject>
#include <QByteArray>
#include <QString>

// Определяем, под какой ОС собираем
#ifdef Q_OS_WIN
// Windows: подключаем windows.h и CH375DLL
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
#include "ch375_linux/demo/ch37x_lib.h"   // путь к файлам из Linux-архива

// В Linux дескриптор – это файловый номер (int)
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
    DeviceHandle m_deviceHandle;   // сохраняем имя переменной
    bool m_isOpen;
    int m_writeTimeout;
    int m_readTimeout;

#ifdef Q_OS_WIN
    // Вспомогательный метод для Windows (безопасное приведение HANDLE к ULONG)
    ULONG handleToULong() const;
#else
    // Для Linux дополнительные данные (информация о конечных точках)
    // Можно хранить прямо в реализации или здесь, если нужно
#endif

    // Общие методы для работы с ответом
    QString waitForResponse(int timeoutMs);
};

#endif // USBINTERFACE_H
