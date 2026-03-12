#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <QWidget>
#include "ppbcontrollerlib.h"

namespace Ui {
class PPBStatus;
}

class StatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatusWidget(QWidget *parent = nullptr);
    ~StatusWidget();

    // Основные методы для обновления данных
    void updateState(const PPBFullState& state, bool showCodes);
    void setDisplayMode(bool codes); // true – коды, false – физические величины

private:
    // Вспомогательные методы форматирования
    QString formatPower(float watts, bool codes) const;
    QString formatTemperature(float celsius, bool codes) const;
    QString formatVSWR(float vswr, bool codes) const;

    // Обновление конкретного groupBox по номеру канала (1 или 2)
    //void updateChannel(int channel, const UIChannelState& state, bool codes);

private:
    Ui::PPBStatus *ui;
    bool m_displayAsCodes = false; // текущий режим отображения
};

#endif
