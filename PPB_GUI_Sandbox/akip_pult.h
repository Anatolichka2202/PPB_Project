#ifndef GENERATORPULT_H
#define GENERATORPULT_H

#include <QWidget>
#include <QElapsedTimer>
#include "iakipcontroller.h"

namespace Ui {
class GeneratorPult;
}

class GeneratorPult : public QWidget
{
    Q_OBJECT

public:
    explicit GeneratorPult(IAkipController *controller, QWidget *parent = nullptr);
    ~GeneratorPult();

    // Запрет копирования
    GeneratorPult(const GeneratorPult&) = delete;
    GeneratorPult& operator=(const GeneratorPult&) = delete;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onChannelChanged();

    void onSetFreqClicked();
    void onQueryFreqClicked();
    void onSetOutputClicked();
    void onQueryOutputClicked();
    void onSetAmplClicked();
    void onQueryAmplClicked();
    void onSetWaveformClicked();
    void onQueryWaveformClicked();
    void onSetDutyClicked();
    void onQueryDutyClicked();
    void onSetAMFreqClicked();
    void onQueryAMFreqClicked();
    void onSetAMDepthClicked();
    void onQueryAMDepthClicked();
    void onSetAMStateClicked();
    void onQueryAMStateClicked();
    void onResetClicked();
    void onSendCustomClicked();

    void onFrequencyChanged(int channel, double freq);
    void onOutputChanged(int channel, bool enabled);
    void onAmplitudeChanged(int channel, double amplitude);
    void onWaveformChanged(int channel, const QString &waveform);
    void onDutyCycleChanged(int channel, double percent);
    void onAmFrequencyChanged(int channel, double freq);
    void onAmDepthChanged(int channel, double percent);
    void onAmStateChanged(int channel, bool enabled);
    void onError(const QString &error);

private:
    Ui::GeneratorPult *ui;
    IAkipController *m_controller;
    QElapsedTimer m_timer;
    int m_currentChannel;  // 1 или 2

    void appendToLog(const QString &text, bool isError = false);
    void updateLastOpTime(qint64 elapsedMs);
    void updateChannelDependentControls();
    bool ensureAvailable() const;
    void closeIfUnavailable();
    void setControlsEnabled(bool enabled);
};

#endif // GENERATORPULT_H
