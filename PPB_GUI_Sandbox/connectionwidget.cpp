#include "connectionwidget.h"
#include "ui_connectionwidget.h"
#include <QMessageBox>
#include <QHostAddress>
#include <QStyle>
ConnectionWidget::ConnectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionWidget)
{
    ui->setupUi(this);

    // Установка значений по умолчанию
    ui->lineEditIPAddress->setText("192.168.0.230");
    ui->lineEditPort->setText("1080");

    // Подключение кнопок
    connect(ui->pushButtonConnect, &QPushButton::clicked, this, [this]() {
        emit bridgePingRequested(ip(), port());
    });

    connect(ui->pushButtonExit, &QPushButton::clicked, this, &ConnectionWidget::exitClicked);
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
    switch (state) {
    case PPBState::Idle:
        buttonText = "Подключиться";
        break;
    case PPBState::Ready:
        if (busy)
            buttonText = "Выполняется операция...";
        else
            buttonText = "Отключиться";
        break;
    default:
        buttonText = "Ожидание...";
        break;
    }
    ui->pushButtonConnect->setText(buttonText);

    // Поля ввода доступны только в Idle
    ui->lineEditIPAddress->setEnabled(state == PPBState::Idle);
    ui->lineEditPort->setEnabled(state == PPBState::Idle);
}

void ConnectionWidget::setBridgeStatus(bool available)
{
    ui->labelConnectionStatus->setProperty("state", available ? "on" : "off");
    ui->labelConnectionStatus->style()->polish(ui->labelConnectionStatus);
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
        emit bridgePingRequested(ip(), p); // теперь это пинг бриджа
    }
    else if (m_currentState == PPBState::Ready) {
        emit disconnectRequested();
    }
}



