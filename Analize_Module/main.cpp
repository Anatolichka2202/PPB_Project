#include "testanalyzer.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

#ifdef Q_OS_WIN
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    std::cout << "=== Тестирование модуля анализатора пакетов ===" << std::endl;
    std::cout << "Протокол: 4-байтные пакеты [data1][data2][index][CRC8]" << std::endl;
    std::cout << "==============================================" << std::endl;

    TestAnalyzer tester;

    try {
        tester.runAllTests();
        std::cout << "\n=== Тестирование завершено ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Исключение: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Неизвестное исключение" << std::endl;
    }

#ifdef Q_OS_WIN
    system("pause");
#endif

    return 0;
}
