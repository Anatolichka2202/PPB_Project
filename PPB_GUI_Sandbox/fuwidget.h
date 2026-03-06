/**
 * @file fuwidget.h
 * @brief Виджет управления формирующим устройством (ФУ).
 *
 * Позволяет выбрать режим (передача/приём), задать длительность и задержку импульса.
 */

#ifndef FUWIDGET_H
#define FUWIDGET_H

#include <QWidget>

namespace Ui {
class FuWidget;
}
/**
 * @class FuWidget
 * @brief Настройка параметров ФУ.
 *
 * Содержит радиокнопки «Передача»/«Приём» и поля ввода длительности и задержки импульса.
 * По умолчанию установлен режим приёма, длительность 27000, задержка 0.
 */
class FuWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FuWidget(QWidget *parent = nullptr);
    ~FuWidget();

    /**
 * @brief Определяет, выбран ли режим передачи.
 * @return true – режим передачи, false – приёма.
 */
    bool isTransmitMode() const;

    /**
 * @brief Возвращает длительность импульса (в мкс?).
 * @return Длительность (uint32_t).
 */
    uint32_t pulseDuration() const;

    /**
 * @brief Возвращает задержку импульса.
 * @return Задержка (uint32_t).
 */
    uint32_t pulseDelay() const;

signals:
    /**
 * @fn void modeChanged(bool transmit)
 * @brief Сигнал изменения режима ФУ.
 * @param transmit true – передача, false – приём.
 *
 * Испускается при переключении радиокнопок.
 */
    void modeChanged(bool transmit);
    void sendFuCommand(bool transmit, uint16_t pulseDuration, uint16_t dutyCycle);
private slots:
    void onTransmitToggled(bool checked);
    void onReceiveToggled(bool checked);

    void on_fuBtnSent_clicked();

private:
    Ui::FuWidget *ui;
};

#endif
