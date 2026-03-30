#ifndef GRATTENCONTROLWIDGET_H
#define GRATTENCONTROLWIDGET_H

#include <QWidget>
#include <QElapsedTimer>
#include "iakipcontroller.h"

namespace Ui {
class GrattenControlWidget;
}

class GrattenControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GrattenControlWidget(IAkipController *controller, QWidget *parent = nullptr);
    ~GrattenControlWidget();

signals:
    void logMessage(const QString &msg); // для передачи в главное окно

private slots:
    void onSetFreqClicked();
    void onQueryFreqClicked();
    void onSetAmplClicked();
    void onQueryAmplClicked();



    void onQueryOutputClicked();
    void onSendCommandClicked();

    void onFrequencyChanged(int channel, double freq);
    void onAmplitudeChanged(int channel, double amplitude);
    void onOutputChanged(int channel, bool enabled);
    void onError(const QString &error);


    void on_botton_onof_clicked();

private:
    void updateOutputUI(bool isOn); // обновление кнопки и лампочки

    Ui::GrattenControlWidget *ui;
    IAkipController *m_controller;
    QElapsedTimer m_timer;

    void updateLastOpTime(qint64 elapsedMs);
    void appendToTerminal(const QString &text, bool isCommand = false, bool isError = false);
};

#endif // GRATTENCONTROLWIDGET_H
