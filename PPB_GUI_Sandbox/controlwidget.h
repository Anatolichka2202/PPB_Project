#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <ppbprotocol.h>
#include "ppbcontrollerlib.h"

namespace Ui {
class ControlWidget;
}

class ControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ControlWidget(QWidget *parent = nullptr);
    ~ControlWidget();

    void setController(PPBController* controller);

    int currentPPBIndex() const;
    void setCurrentPPBIndex(int index);
    void setAutoPollChecked(bool checked);
    void setBusy(bool busy);
    void setConnected(bool connected);

public slots:
    void refreshSettings(); // принудительное обновление полей из настроек

signals:
    void pollStatusClicked();
    void resetClicked();
    void testSequenceClicked();
    void autoPollToggled(bool checked);
    void ppbSelected(int index);

private slots:
    void onPollClicked();
    void onResetClicked();
    void onTestClicked();
    void onAutoPollToggled(bool checked);
    void onComboBoxIndexChanged(int index);
    void onPower1Changed(const QString& text);
    void onPower2Changed(const QString& text);
    void onFuBlockedToggled(bool checked);
    void onRebootToggled(bool checked);
    void onResetErrorsToggled(bool checked);
    void onPowerToggled(bool checked);

    // Новые слоты для работы с настройками
    void onUserSettingsChanged(uint8_t ppbIndex);

    // Оставляем для возможного использования, но не обновляем поля ввода
    void onFullStateUpdated(uint8_t ppbIndex);

private:
    uint16_t getSelectedAddress() const;

    Ui::ControlWidget *ui;
    PPBController* m_controller;
    bool m_busy = false;
    bool m_connected = false;
};

#endif // CONTROLWIDGET_H
