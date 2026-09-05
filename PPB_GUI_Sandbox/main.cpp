#include <QApplication>
#include <QTimer>
#include <QMessageBox>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QCommandLineParser>

#include "applicationmanager.h"
#include "testerwindow.h"
#include "thememanager.h"
#include "updatemanager.h"

#ifdef Q_OS_LINUX
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

void signalHandler(int sig, siginfo_t *info, void *context) {
    Q_UNUSED(info)
    Q_UNUSED(context)

    const char msg1[] = "\n*** Caught signal ";
    write(STDERR_FILENO, msg1, sizeof(msg1)-1);

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", sig);
    write(STDERR_FILENO, buf, len);

    const char msg2[] = " (";
    write(STDERR_FILENO, msg2, sizeof(msg2)-1);

    const char* sigstr = strsignal(sig);
    if (sigstr) {
        write(STDERR_FILENO, sigstr, strlen(sigstr));
    }

    const char msg3[] = ") ***\n";
    write(STDERR_FILENO, msg3, sizeof(msg3)-1);

    void* array[20];
    int size = backtrace(array, 20);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    signal(sig, SIG_DFL);
    raise(sig);
}

void setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_sigaction = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}
#endif

#ifdef Q_OS_WIN
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
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
#endif

#ifdef Q_OS_LINUX
    setupSignalHandlers();
#endif

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("PPB Tester");
    QCoreApplication::setOrganizationName("MILTECH");
    QCoreApplication::setApplicationVersion(QStringLiteral(PPB_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/ppb-icon.svg")));

    auto& themeManager = ThemeManager::instance();
    Q_UNUSED(themeManager)

    QCommandLineParser parser;
    parser.setApplicationDescription("PPB Tester");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption testOption("test", "Run in test mode (use mock generator)");
    QCommandLineOption noUpdateOption("no-update-check", "Disable automatic GitHub Releases update check");
    parser.addOption(testOption);
    parser.addOption(noUpdateOption);
    parser.process(app);

    auto& manager = ApplicationManager::instance();
    const bool testMode = parser.isSet(testOption);
    const bool updateChecksEnabled = !testMode && !parser.isSet(noUpdateOption);

    if (testMode) {
        manager.enableTestMode(true);
    }

    auto* updateManager = new UpdateManager(&app);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        manager.shutdown();
    });

    QObject::connect(&manager, &ApplicationManager::initializationComplete,
                     &app, [&, updateManager, updateChecksEnabled]() {
                         LOG_TECH_DEBUG("initializationComplete received");
                         auto* window = manager.mainWindow();
                         if (window) {
                             window->show();
                             LOG_TECH_DEBUG("Main window shown");

                             if (updateChecksEnabled) {
                                 QTimer::singleShot(1500, updateManager, [updateManager]() {
                                     updateManager->checkForUpdates(false);
                                 });
                             }
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
        manager.initialize();
    });

    int result;
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

    return result;
}
