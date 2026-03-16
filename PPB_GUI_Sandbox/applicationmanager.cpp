#include "applicationmanager.h"
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QMessageBox>
#include "../PPBCommunicationLib/src/communicationfacade.h"
#include "../PPBControllerLib/include/ppbcontrollerlib.h"
#include "packetanalyzer_interface.h"
#include "testerwindow.h"
#include "akipfacade.h"           // для создания конкретного экземпляра
#include "grattenga1483controller.h"
#include <QAbstractButton>
#include <QPushButton>
ApplicationManager* ApplicationManager::m_instance = nullptr;

ApplicationManager& ApplicationManager::instance()
{
    if (!m_instance) {
        m_instance = new ApplicationManager(qApp);
    }
    return *m_instance;
}

ApplicationManager::ApplicationManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_commThread(nullptr)
{
}

ApplicationManager::~ApplicationManager()
{
    shutdown();
}

bool ApplicationManager::initialize()
{
    if (m_initialized) return true;

    try {
        //0. инициализируем логирование
        LogWrapper::instance();
       // LOG_TECH_DEBUG("LogWrapper initialized");

        // 1. Создаём коммуникационный поток
        m_commThread = new QThread(this);
        m_commThread->setObjectName("CommunicationThread");
       // LOG_TECH_DEBUG("Communication thread created");

        // 2. Инициализация коммуникации
        initializeCommunication();
       // LOG_TECH_DEBUG("Communication initialized");

        // 3. Инициализация анализатора пакетов
        initializeAnalyzer();
       // LOG_TECH_DEBUG("Analyzer initialized");

        // 4.1 Инициализация контроллера
        initializeController();
//LOG_TECH_DEBUG("Controller initialized");

        //4.2 Инициализация генератора
        detectAndSelectGenerator();
        if (m_signalGenerator) {
            m_controller->setAkipInterface(m_signalGenerator.get());
            // Не вызываем методы окна – оно ещё не создано!
        }

        // 5. Инициализация главного окна (теперь оно точно существует)
        initializeMainWindow();

        // Теперь можно передать генератор в окно
        if (m_signalGenerator) {
            m_mainWindow->setSignalGeneratorController(m_signalGenerator.get());
        } else {
            m_mainWindow->setSignalGeneratorController(nullptr);
        }

        m_initialized = true;
        emit initializationComplete();
        return true;
    }
    catch (const std::exception& e) {
        emit initializationFailed(e.what());
        cleanup();
        return false;
    }
}
void ApplicationManager::initializeCommunication()
{
    // Создаём временный unique_ptr с делитером по умолчанию
    auto facade = std::make_unique<CommunicationFacade>();
    facade->moveToThread(m_commThread);

    m_commThread->start();

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(5000);

    bool initSuccess = false;
    QString errorMsg;

    connect(facade.get(), &CommunicationFacade::initialized, &loop, [&]() {
        initSuccess = true;
        loop.quit();
    });
    connect(facade.get(), &CommunicationFacade::errorOccurred, &loop, [&](const QString& err) {
        errorMsg = err;
        loop.quit();
    });
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        errorMsg = "Timeout initializing communication";
        loop.quit();
    });

    QMetaObject::invokeMethod(facade.get(), "initialize", Qt::QueuedConnection);

    loop.exec();

    if (!initSuccess) {
       // LOG_UI_ALERT(errorMsg);
        // Ошибка: удаляем объект в его потоке через deleteLater
        QMetaObject::invokeMethod(
            facade.get(),
            "deleteLater",
            Qt::QueuedConnection
            );
        facade.release(); // предотвращаем удаление в главном потоке
        throw std::runtime_error(errorMsg.toStdString());
    }

    // Успех: передаём владение в m_communication с правильным делитером
    m_communication = std::unique_ptr<CommunicationFacade, DeleteLaterDeleter>(
        facade.release()
        );
}

void ApplicationManager::initializeAnalyzer()
{
    // Здесь создаём реальный анализатор или заглушку
    // Пока используем заглушку (например, StubPacketAnalyzer)
    class StubAnalyzer : public PacketAnalyzerInterface {
    public:
        void addSentPackets(const QVector<DataPacket>&) override {}
        void addReceivedPackets(const QVector<DataPacket>&) override {}
        void clear() override {}
        void analyze() override {}
        void setCheckCRC(bool) override {}
        void setMaxReorderingWindow(int) override {}
        int sentCount() const override { return 0; }
        int receivedCount() const override { return 0; }
    };
    m_analyzer = std::make_unique<StubAnalyzer>();
}

void ApplicationManager::initializeController()
{
    if (!m_communication) {
        throw std::runtime_error("Communication not initialized");
    }
    if (!m_analyzer) {
        throw std::runtime_error("Analyzer not initialized");
    }

    // Создаём контроллер, передавая фасад и анализатор
    m_controller = std::make_unique<PPBController>(m_communication.get(), m_analyzer.get());
}

void ApplicationManager::initializeMainWindow()
{
    if (!m_controller) {
        throw std::runtime_error("Controller not initialized");
    }

    //LOG_TECH_DEBUG("Creating TesterWindow...");
    m_mainWindow = std::make_unique<TesterWindow>(m_controller.get());
   // LOG_TECH_DEBUG("TesterWindow created");

    m_mainWindow->setWindowIcon(QIcon(":/icons/bagger.png"));
  //  LOG_TECH_DEBUG("Icon set");

   // LOG_TECH_DEBUG("Main window created and set");
}

void ApplicationManager::shutdown()
{
    if (!m_initialized) return;

    m_mainWindow.reset();
    m_controller.reset();

    if (m_communication) {
        m_communication->disconnect();
        m_communication->deleteLater();
        m_communication.release();
    }

    if (m_commThread && m_commThread->isRunning()) {
        m_commThread->quit();
        if (!m_commThread->wait(3000)) {
            m_commThread->terminate();
            m_commThread->wait();
        }
        delete m_commThread;
        m_commThread = nullptr;
    }

    m_initialized = false;
}

void ApplicationManager::cleanup()
{
    // Аварийная очистка – максимально быстро освобождаем ресурсы
    m_mainWindow.reset();
    m_controller.reset();

    if (m_communication) {
        m_communication->disconnect();
        m_communication->deleteLater();
        m_communication.release();
    }

    if (m_commThread && m_commThread->isRunning()) {
        m_commThread->quit();
        m_commThread->wait(1000);
        m_commThread->terminate();
        m_commThread->wait();
        delete m_commThread;
        m_commThread = nullptr;
    }

    m_initialized = false;
}

void ApplicationManager::detectAndSelectGenerator()
{
    bool akipOk = false;
    bool grattenOk = false;
    QString akipIdn, grattenIdn;

    // Проверка АКИП (USB)
    {
        AkipFacade testAkip;
        if (testAkip.openDevice(0)) {
            akipIdn = testAkip.getIdentity();
            akipOk = !akipIdn.isEmpty();
            testAkip.closeDevice();
        }
    }

    // Проверка Gratten (LAN, IP по умолчанию, можно вынести в настройки)
    const QString grattenIp = "192.168.0.66"; // TODO: брать из конфига
    {
        GrattenGa1483Controller testGratten;
        testGratten.setConnectionParameters(grattenIp, 5025);
        if (testGratten.openDevice()) {
            grattenIdn = testGratten.getIdentity();
            grattenOk = !grattenIdn.isEmpty();
            testGratten.closeDevice();
        }
    }

    if (akipOk && grattenOk) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Выбор генератора");
        msgBox.setText("Обнаружено несколько устройств. Выберите:");

        // Сохраняем кнопки как QPushButton*
        QPushButton* btnAkip = msgBox.addButton("АКИП-3417 (USB)", QMessageBox::AcceptRole);
        QPushButton* btnGratten = msgBox.addButton("Gratten GA1483 (LAN)", QMessageBox::AcceptRole);
        msgBox.addButton(QMessageBox::Cancel);
        msgBox.exec();

        QAbstractButton* clicked = msgBox.clickedButton();

        // Используем qobject_cast для приведения clicked к QPushButton*
        if (qobject_cast<QPushButton*>(clicked) == btnAkip) {
            m_signalGenerator = std::unique_ptr<IAkipController, DeleteLaterDeleter>(new AkipFacade());
        } else if (qobject_cast<QPushButton*>(clicked) == btnGratten) {
            auto gratten = new GrattenGa1483Controller();
            gratten->setConnectionParameters(grattenIp, 5025);
            m_signalGenerator = std::unique_ptr<IAkipController, DeleteLaterDeleter>(gratten);
        } else {
            LOG_UI_STATUS("Выбор генератора отменён, ручное управление.");
            return;
        }
    }
    else if (akipOk) {
        m_signalGenerator = std::unique_ptr<IAkipController, DeleteLaterDeleter>(new AkipFacade());
    }
    else if (grattenOk) {
        auto gratten = new GrattenGa1483Controller();
        gratten->setConnectionParameters(grattenIp, 5025);
        m_signalGenerator = std::unique_ptr<IAkipController, DeleteLaterDeleter>(gratten);
    }
    else {
        LOG_UI_ALERT("Ни одно устройство не найдено. Ручное управление.");
        return;
    }

    if (!m_signalGenerator->openDevice()) {
        LOG_UI_ALERT("Не удалось открыть устройство");
        m_signalGenerator.reset();
        return;
    }

    LOG_UI_STATUS(QString("Подключён генератор: %1").arg(m_signalGenerator->getIdentity()));
    configureGenerator();
    logGeneratorState();
}

void ApplicationManager::configureGenerator()
{
    if (!m_signalGenerator) return;

    // Для АКИП выполняем специальную настройку
    if (dynamic_cast<AkipFacade*>(m_signalGenerator.get())) {
        auto akip = dynamic_cast<AkipFacade*>(m_signalGenerator.get());
        // Выключаем выходы
        akip->setOutput(1, false);
        akip->setOutput(2, false);
        QThread::msleep(20);

        akip->setWaveform(2, "SQUARE");
        akip->setAmplitude(2, 4.0, "VPP");
        akip->setDutyCycle(2, 33.333);

        akip->setFrequency(1, 500000000.0);
        akip->setAmplitude(1, 0.0, "DBM");
        akip->setAMFrequency(1, 20000000.0);
        akip->setAMDepth(1, 100.0);
        akip->sendCommand("AM:SOUR INT");
        // Выходы не включаем
    }
    // Для Gratten можно добавить аналогичную настройку, если требуется
}

void ApplicationManager::reconnectGenerator()
{
    if (m_signalGenerator) {
        // Если хотим мягко закрыть, можно вызвать closeDevice() перед удалением
        // Но unique_ptr с DeleteLaterDeleter сам вызовет deleteLater при сбросе.
        m_signalGenerator.reset();
    }
    detectAndSelectGenerator();
    if (m_signalGenerator && m_mainWindow) {
        m_mainWindow->setSignalGeneratorController(m_signalGenerator.get());
    }
}

void ApplicationManager::logGeneratorState()
{
    if (!m_signalGenerator || !m_signalGenerator->isAvailable()) return;

    // Однократно запрашиваем и кэшируем идентификацию
    static QString cachedIdn;
    if (cachedIdn.isEmpty()) {
        cachedIdn = m_signalGenerator->getIdentity();
    }
    LOG_UI_STATUS("Состояние генератора: " + cachedIdn);
}
