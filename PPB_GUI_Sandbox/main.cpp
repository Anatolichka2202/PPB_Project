#include <QApplication>
#include <QTimer>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QDebug>

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include "applicationmanager.h"
#include "testerwindow.h"

#ifdef Q_OS_WIN
// Обработчик необработанных исключений Windows
LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo)
{
    QString errorMsg;
    quintptr address = (quintptr)exceptionInfo->ExceptionRecord->ExceptionAddress;

    switch (exceptionInfo->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        errorMsg = "Нарушение доступа к памяти (Access Violation)";
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        errorMsg = "Выход за границы массива";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        errorMsg = "Переполнение стека";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        errorMsg = "Деление на ноль";
        break;
    default:
        errorMsg = QString("Код исключения: 0x%1").arg(exceptionInfo->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'));
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [FATAL] Необработанное исключение Windows: %2, адрес: 0x%3")
                             .arg(timestamp).arg(errorMsg).arg(address, 16, 16, QChar('0'));

    // Запись в файл crash.log
    QFile logFile("crash.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << logMessage << "\n";
        logFile.close();
    }

    qCritical() << logMessage;

    QMessageBox::critical(nullptr, "Критическая ошибка",
                          QString("Произошла критическая ошибка:\n%1\n\nПрограмма будет закрыта.")
                              .arg(errorMsg));

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char *argv[])
{
    #ifdef Q_OS_WIN
    // Установка обработчика необработанных исключений Windows
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
    #endif

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("PPB Tester");
    QCoreApplication::setOrganizationName("YourCompany");

    auto& manager = ApplicationManager::instance();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        manager.shutdown();
    });

    QObject::connect(&manager, &ApplicationManager::initializationComplete,
                     &app, [&]() {
                         LOG_TECH_DEBUG("initializationComplete received");
                         auto* window = manager.mainWindow();
                         if (window) {
                             window->show();
                             LOG_TECH_DEBUG("Main window shown");
                         } else {
                             qCritical() << "Main window is null";
                             app.quit();
                         }
                     });
    QObject::connect(&manager, &ApplicationManager::initializationFailed,
                     &app, [&](const QString& error) {
                         QMessageBox::critical(nullptr, "Initialization Error",
                                               "Failed to initialize application:\n" + error);
                         app.quit();
                     });

    QTimer::singleShot(0, [&]() {
        if (!manager.initialize()) {
            // initialize() сам пошлёт сигнал initializationFailed
        }
    });

    // Защита от C++ исключений
    int result;
    //LOG_TECH_DEBUG("Entering app.exec()");
    try {
        result = app.exec();
    } catch (const std::exception& e) {
        qCritical() << "Стандартное исключение:" << e.what();
        QMessageBox::critical(nullptr, "Исключение", QString("Стандартное исключение: %1").arg(e.what()));
        result = 1;
    } catch (...) {
        qCritical() << "Неизвестное исключение";
        QMessageBox::critical(nullptr, "Исключение", "Неизвестное исключение");
        result = 1;
    }

    //LOG_TECH_DEBUG("app.exec() exited");
    return result;
}
