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
    /**
 * @brief Возвращает индекс выбранного ППБ в комбобоксе.
 * @return Индекс от 0 до 15.
 */
    int currentPPBIndex() const;

    /**
 * @brief Принудительно устанавливает индекс выбранного ППБ.
 * @param index Новый индекс (должен быть в диапазоне 0..15).
 */
    void setCurrentPPBIndex(int index);

    /**
 * @brief Устанавливает состояние флажка «Автоопрос».
 * @param checked true – включить, false – выключить.
 *
 * Используется для синхронизации с состоянием контроллера.
 */
    void setAutoPollChecked(bool checked);

    /**
 * @brief Устанавливает флаг занятости контроллера.
 * @param busy true – контроллер выполняет операцию, false – свободен.
 *
 * Блокирует или разблокирует кнопки и чекбокс.
 */
    void setBusy(bool busy);

    /**
 * @brief Устанавливает состояние подключения.
 * @param connected true – есть соединение, false – нет.
 *
 * Влияет на доступность кнопок (вместе с busy).
 */
    void setConnected(bool connected);
signals:
    /**
 * @fn void pollStatusClicked()
 * @brief Запрос на опрос состояния выбранного ППБ.
 */

    /**
 * @fn void resetClicked()
 * @brief Запрос на сброс выбранного ППБ.
 */

    /**
 * @fn void testSequenceClicked()
 * @brief Запрос на запуск полного теста выбранного ППБ.
 */

    /**
 * @fn void autoPollToggled(bool checked)
 * @brief Сигнал изменения состояния автоопроса.
 * @param checked true – включить, false – выключить.
 */

    /**
 * @fn void ppbSelected(int index)
 * @brief Сигнал о выборе другого ППБ в комбобоксе.
 * @param index Новый индекс.
 */
    void pollStatusClicked();
    void resetClicked();
    void testSequenceClicked();
    void autoPollToggled(bool checked);
    void ppbSelected(int index);                // выбран другой ППБ

private slots:
    void onPollClicked();
    void onResetClicked();
    void onTestClicked();
    void onAutoPollToggled(bool checked);
    void onComboBoxIndexChanged(int index);


private:
    Ui::ControlWidget *ui;
    bool m_busy = false;
    bool m_connected = false;
};

#endif
