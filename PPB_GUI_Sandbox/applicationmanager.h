#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include <QObject>
#include <QThread>
#include <memory>
#include "iakipcontroller.h"
#include "loguimanager.h"

// Forward declarations
class CommunicationFacade;
class PPBController;
class TesterWindow;
class PacketAnalyzerInterface;

struct DeleteLaterDeleter {
    void operator()(QObject* obj) const { if (obj) obj->deleteLater(); }
};

class ApplicationManager : public QObject
{
    Q_OBJECT
public:
    static ApplicationManager& instance();
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    TesterWindow* mainWindow() const { return m_mainWindow.get(); }
    PPBController* controller() const { return m_controller.get(); }
    CommunicationFacade* communication() const { return m_communication.get(); }

    void reconnectGenerator();
signals:
    void initializationComplete();
    void initializationFailed(const QString& error);

private:
    explicit ApplicationManager(QObject* parent = nullptr);
    ~ApplicationManager();

    void initializeCommunication();
    void initializeAnalyzer();
    void initializeController();
    void initializeMainWindow();
    void cleanup();

    // Методы для работы с генератором
    void detectAndSelectGenerator();          // обнаружение и выбор устройства
    void configureGenerator();                 // начальная настройка (если нужно)
    void logGeneratorState();                  // логирование состояния


private:
    static ApplicationManager* m_instance;
    bool m_initialized;
    QThread* m_commThread;

    std::unique_ptr<CommunicationFacade, DeleteLaterDeleter> m_communication;
    std::unique_ptr<PacketAnalyzerInterface> m_analyzer;
    std::unique_ptr<PPBController> m_controller;
    std::unique_ptr<TesterWindow> m_mainWindow;

    // Унифицированный указатель на контроллер генератора
    std::unique_ptr<IAkipController, DeleteLaterDeleter> m_signalGenerator;
};

#endif // APPLICATIONMANAGER_H
