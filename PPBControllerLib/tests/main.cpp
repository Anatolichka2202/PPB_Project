#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "controller_dependencies.h"  // подключает STUBS.h в режиме заглушек
#include "ppbcontrollerlib.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== PPBController Test (with STUBS) ===";

    // Создаём заглушки (классы из STUBS.h, реализующие интерфейсы)
    PPBCommunication comm;
    StubPacketAnalyzer analyzer;

    // Создаём контроллер, передавая указатели на заглушки
    PPBController controller(&comm, &analyzer);

    // Подключаем сигналы для проверки
    QObject::connect(&controller, &IController::connectionStateChanged, [](PPBState state) {
        qDebug() << "State changed:" << static_cast<int>(state);
    });

    QObject::connect(&controller, &IController::statusReceived, [](uint16_t addr, const QVector<QByteArray>& data) {
        qDebug() << "Status received from" << addr << "size:" << data.size();
    });

    // Тестируем подключение
    controller.connectToPPB(0x0001, "127.0.0.1", 1234);

    // Запрашиваем статус через таймер
    QTimer::singleShot(1000, [&]() {
        qDebug() << "Requesting status...";
        controller.requestStatus(0x0001);
    });

    // Тестируем анализ пакетов
    QTimer::singleShot(2000, [&]() {
        qDebug() << "Running analysis...";
        controller.analize();
    });

    // Завершаем через 3 секунды
    QTimer::singleShot(3000, &app, &QCoreApplication::quit);

    return app.exec();
}
