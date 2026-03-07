/**
 * @file controlwidget.h
 * @brief Виджет управления выбором ППБ и основными командами.
 *
 * Позволяет выбрать номер ППБ (0-15), выполнить опрос состояния, сброс,
 * тестовую последовательность, а также включить/выключить автоопрос.
 * Блокируется при отсутствии подключения или занятости контроллера.
 */
#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <ppbprotocol.h>
#include "ppbcontrollerlib.h"
namespace Ui {
class ControlWidget;
}
/**
* @class ControlWidget
     * @brief Панель управления ППБ.
         *
             * Содержит комбобокс выбора ППБ, кнопки «Опрос», «Сброс», «Тест»,
    * чекбокс «Автоопрос». Все элементы (кроме комбобокса) блокируются,
    * если нет подключения или идёт операция.
            */
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

signals:
    void pollStatusClicked();
    void resetClicked();                  // теперь без данных
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
    void onFullStateUpdated(uint8_t ppbIndex);

private:
    uint16_t getSelectedAddress() const;

    Ui::ControlWidget *ui;
    PPBController* m_controller;
    bool m_busy = false;
    bool m_connected = false;
};
#endif
