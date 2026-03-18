#ifndef CONNECTIONWIDGET_H
#define CONNECTIONWIDGET_H

#include <QWidget>
#include "ppbcontrollerlib.h" // для PPBState

namespace Ui {
class ConnectionWidget;
}

class ConnectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget *parent = nullptr);
    ~ConnectionWidget();

    QString ip() const;
    quint16 port() const;
    bool allPpbChecked() const; // состояние чекбокса "Все ппб"

public slots:
    // Обновляет состояние подключения бриджа (текст кнопки, доступность)
    void setConnectionState(PPBState state, bool busy);
    // Устанавливает цвет индикатора бриджа
    void setBridgeStatus(bool available);

signals:
    // Запрос на проверку доступности бриджа (ping)
    void bridgePingRequested(const QString &ip, quint16 port);
    // Запрос на отключение (если нужно)
    void disconnectRequested();
    // Запрос на выход из программы
    void exitClicked();

private slots:
    void onConnectButtonClicked();

private:
    Ui::ConnectionWidget *ui;
    PPBState m_currentState = PPBState::Idle;
};

#endif // CONNECTIONWIDGET_H
