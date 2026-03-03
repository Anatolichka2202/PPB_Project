#include "connectionwidget.h"
#include "ui_connectionwidget.h"
#include <QMessageBox>
#include <QHostAddress>
ConnectionWidget::ConnectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionWidget)
{
    ui->setupUi(this);

    // Установка значений по умолчанию
    ui->lineEditIPAddress->setText("198.168.0.230");
    ui->lineEditPort->setText("1080");

    // Подключение кнопки
    connect(ui->pushButtonConnect, &QPushButton::clicked,
            this, &ConnectionWidget::onConnectButtonClicked);
    connect(ui->pushButtonExit, &QPushButton::clicked, this, &ConnectionWidget::exitClicked);
    qDebug() << "Constructor of ConnectionWidget";
}

ConnectionWidget::~ConnectionWidget()
{
    delete ui;
}

QString ConnectionWidget::ip() const
{
    return ui->lineEditIPAddress->text();
}

quint16 ConnectionWidget::port() const
{
    return ui->lineEditPort->text().toUShort();
}

void ConnectionWidget::setConnectionState(PPBState state, bool busy)
{
    m_currentState = state;

    QString buttonText;
    QString indicatorStyle;
    QString tooltip;
    switch (state) {
    case PPBState::Idle:
        indicatorStyle = "border-radius: 10px; border: 2px solid #666; background-color: #ff0000;";
        buttonText = "Подключиться";
        tooltip = "Отключено";
        break;

    case PPBState::Ready:
        if (busy) {
            indicatorStyle = "border-radius: 10px; border: 2px solid #666; background-color: #ffff00;";
            buttonText = "Выполняется операция...";
            tooltip = "Выполняется операция";
        } else {
            indicatorStyle = "border-radius: 10px; border: 2px solid #666; background-color: #00ff00;";
            buttonText = "Отключиться";
            tooltip = "Подключено";
        }
        break;

    case PPBState::SendingCommand:


    default:
        indicatorStyle = "border-radius: 10px; border: 2px solid #666; background-color: #cccccc;";
        buttonText = "Ожидание...";
        tooltip = "Неизвестное состояние";
        break;
    }
    ui->pushButtonConnect->setText(buttonText);
    ui->labelConnectionStatus->setStyleSheet(indicatorStyle);
    ui->labelConnectionStatus->setToolTip(tooltip);

    // Поля ввода доступны только в состоянии Idle и когда не busy Лучше управлять из главного окна, но можно и здесь
    ui->lineEditIPAddress->setEnabled(state == PPBState::Idle);
    ui->lineEditPort->setEnabled(state == PPBState::Idle);
}

void ConnectionWidget::onConnectButtonClicked()
{
    if (m_currentState == PPBState::Idle) {
        QHostAddress addr;
        if (!addr.setAddress(ip())) {
            QMessageBox::warning(this, "Ошибка", "Неверный IP-адрес");
            return;
        }
        quint16 p = port();
        if (p == 0) {
            QMessageBox::warning(this, "Ошибка", "Неверный порт");
            return;
        }
        emit connectRequested(ip(), p);
    }
    else if (m_currentState == PPBState::Ready) {
        emit disconnectRequested();
    }
    // В других состояниях (SendingCommand, WaitingData) кнопка должна быть заблокирована, но на всякий случай ничего не делаем
}
