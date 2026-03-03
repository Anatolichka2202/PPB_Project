/**
 * @file connectionwidget.h
 * @brief Виджет управления подключением к ППБ через Ethernet-бридж.
 *
 * Позволяет ввести IP-адрес и порт, отображает состояние подключения
 * с помощью цветного индикатора и текста на кнопке.
 */
#ifndef CONNECTIONWIDGET_H
#define CONNECTIONWIDGET_H

#include <QWidget>
#include "ppbcontrollerlib.h" // для PPBState

namespace Ui {
class ConnectionWidget;
}
/**
 * @class ConnectionWidget
 * @brief Виджет для установки/разрыва соединения и индикации состояния.
 *
 * Включает поля ввода IP и порта, кнопку подключения/отключения и
 * светодиодный индикатор. Состояние виджета синхронизируется с состоянием
 * контроллера через слот setConnectionState().
 */
class ConnectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget *parent = nullptr);
    ~ConnectionWidget();

    // Получить текущие значения (для инициализации)
    /**
 * @brief Возвращает текущий IP-адрес из поля ввода.
 * @return Строка с IP (например, "192.168.0.230").
 */
    QString ip() const;
    /**
 * @brief Возвращает текущий порт из поля ввода.
 * @return Номер порта (quint16).
 */
    quint16 port() const;


public slots:
    // Обновить состояние подключения
    /**
    * @brief Обновляет внешний вид виджета согласно состоянию подключения.
         * @param state Текущее состояние PPBState (Idle, Ready, SendingCommand, WaitingData).
                 * @param busy Флаг занятости контроллера (true – выполняется операция).
                 *
                     * Меняет текст кнопки, цвет индикатора и доступность полей ввода.
                */
    void setConnectionState(PPBState state, bool busy);

signals:
    /**
 * @fn void connectRequested(const QString& ip, quint16 port)
 * @brief Запрос на подключение к указанному IP и порту.
 * @param ip Адрес Ethernet-бриджа.
 * @param port Порт для подключения.
 *
 * Испускается при нажатии кнопки «Подключиться» в состоянии Idle.
 */

    /**
 * @fn void disconnectRequested()
 * @brief Запрос на отключение от текущего устройства.
 *
 * Испускается при нажатии кнопки «Отключиться» в состоянии Ready.
 */

    /**
 * @fn void exitClicked()
 * @brief Сигнал завершения работы приложения.
 * @sa TesterWindow::onExitClicked()
 */
    // Сигналы для главного окна
    void connectRequested(const QString& ip, quint16 port);
    void disconnectRequested();
     void exitClicked();
private slots:
    void onConnectButtonClicked();

private:
    Ui::ConnectionWidget *ui;
    PPBState m_currentState = PPBState::Idle;
};

#endif // CONNECTIONWIDGET_H
