#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include "iakipcontroller.h"
#include "akipfacade.h"
#include "grattenga1483controller.h"
#include "grattencontrolwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Подключение
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_btnIdn_clicked();

    // Канал A
    void on_btnSetFreqA_clicked();
    void on_btnQueryFreqA_clicked();
    void on_btnSetOutputA_clicked();
    void on_btnQueryOutputA_clicked();
    void on_btnSetAmplA_clicked();
    void on_btnQueryAmplA_clicked();
    void on_btnSetWaveA_clicked();
    void on_btnQueryWaveA_clicked();

    // Канал B
    void on_btnSetFreqB_clicked();
    void on_btnQueryFreqB_clicked();
    void on_btnSetOutputB_clicked();
    void on_btnQueryOutputB_clicked();
    void on_btnSetAmplB_clicked();
    void on_btnQueryAmplB_clicked();
    void on_btnSetWaveB_clicked();
    void on_btnQueryWaveB_clicked();

    // Тестирование задержек
    void on_btnTestIdn_clicked();
    void on_btnTestSetFreq_clicked();
    void on_btnTestQueryFreq_clicked();
    void on_btnTestSeriesIdn_clicked();

    // Лог
    void on_btnLogClear_clicked();
    void on_btnLogSave_clicked();

    // Слоты от контроллера (новые)
    void onControllerAvailabilityChanged(bool available);
    void onControllerError(const QString &error);
    void onControllerFrequencyChanged(int channel, double freq);
    void onControllerOutputChanged(int channel, bool enabled);
    void onControllerAmplitudeChanged(int channel, double amplitude);
    void onControllerWaveformChanged(int channel, const QString &waveform);

private:
    void logMessage(const QString &msg);
    void updateLastOpTimeLabel(int channel, qint64 elapsedMs);
    void setChannelControlsEnabled(bool enabled);
    void updateConnectionStatus();

    enum DeviceType { Unknown, AKIP, GRATTEN };
    IAkipController *m_controller;
    DeviceType m_currentType;
    void checkAvailableDevices();
    void switchToDevice(DeviceType type);
    void showDeviceSelectionDialog(const QString &akipIdn, const QString &grattenIdn);

    Ui::MainWindow *ui;
    QElapsedTimer m_timer;

    QWidget *m_akipPage;
    GrattenControlWidget *m_grattenPage;
    void setupForDeviceType(DeviceType type);
};

#endif // MAINWINDOW_H
