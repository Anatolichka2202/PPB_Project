#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Настройка имени приложения (опционально)
    QCoreApplication::setApplicationName("Akip Test Bench");
    QCoreApplication::setOrganizationName("TestLab");

    MainWindow window;
    window.show();

    return app.exec();
}
