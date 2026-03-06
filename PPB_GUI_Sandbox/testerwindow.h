/**
 * @file testerwindow.h
 * @brief Главное окно приложения.
 *
 * Собирает все виджеты в единый интерфейс: панель подключения, управления,
 * вкладки с состоянием ППБ, панели АКИП, ФУ, сценариев и логов.
 * Управляет взаимодействием между виджетами и контроллером.
 */

#ifndef TESTERWINDOW_H
#define TESTERWINDOW_H

#include <QMainWindow>
#include <QGroupBox>
#include <QStackedWidget>
#include "grattencontrolwidget.h"
#include "iakipcontroller.h"
#include "statuswidget.h"
#include "pult.h"

QT_BEGIN_NAMESPACE
namespace Ui { class TesterWindow; }
QT_END_NAMESPACE
/**
 * @class TesterWindow
 * @brief Главное окно программы управления ППБ.
 *
 * Является центральным элементом GUI. Инициализирует интерфейс,
 * соединяет сигналы виджетов с методами контроллера и обрабатывает
 * ответы от контроллера, обновляя отображение.
 */
class TesterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TesterWindow(PPBController* controller = nullptr, QWidget *parent = nullptr);
    ~TesterWindow() override;
    void setSignalGeneratorController(IAkipController* ctrl); ///< Установка контроллера генератора
protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
 * @name Слоты UI
 * @{
 */
    void onPollStatusClicked();          ///< Запрос статуса (от устаревшей кнопки)
    void onResetClicked();               ///< Сброс (устаревшая кнопка)
    void onApplyParametersClicked();     ///< Применить параметры АКИПа на ФУ
    void onAutoPollToggled(bool checked);///< Переключение автоопроса
    void onTestSequenceClicked();        ///< Запуск полного теста
    void onDisplayModeChanged(bool showCodes); ///< Смена режима отображения
    void onPPBSelected(int index);       ///< Выбор ППБ в комбобоксе
    void onExitClicked();                ///< Выход из приложения
    void onLoadScenario();               ///< Загрузить сценарий
    void onRunScenario();                ///< Запустить сценарий
    void onStopScenario();               ///< Остановить сценарий
    void onPRBSM2SClicked();             ///< PRBS M2S (устаревшая кнопка)
    void onPRBSS2MClicked();             ///< PRBS S2M (устаревшая кнопка)
    void on_pult_active_clicked();       ///< Открыть окно пульта
    void onAkipApplyClicked();           ///< применить к генератору АКИП
    void onAkipAvailabilityChanged(bool available); ///< пизменение параметров
     void onGeneratorAvailabilityChanged(bool available); ///< реакция на отключение
        void on_btnPerformance_clicked(); ///< слот кнопки метрик
    /** @} */

    /**
 * @name Слоты от контроллера
 * @{
 */
    void onControllerConnectionStateChanged(PPBState state);        ///< Изменение состояния подключения
    void onControllerStatusReceived(uint16_t address, const QVector<QByteArray>& data); ///< Получен статус
    void onControllerErrorOccurred(const QString& error);           ///< Ошибка
    void onControllerChannelStateUpdated(uint8_t ppbIndex, int channel, const UIChannelState& state); ///< Обновление канала
    void onOperationProgress(int current, int total, const QString& operation); ///< Прогресс
    void onOperationCompleted(bool success, const QString& message);///< Завершение операции
    void onControllerBusyChanged(bool busy);                         ///< Изменение занятости
    void onTabChanged(int index);                                    ///< Переключение вкладки с ППБ
    /** @} */
private:
    /**
 * @brief Инициализация пользовательского интерфейса.
 */
    void initializeUI();

    /**
 * @brief Соединение сигналов виджетов и контроллера.
 */
    void connectSignals();

    /**
 * @brief Обновление элементов управления согласно состоянию подключения.
 */
    void updateConnectionUI();

    /**
 * @brief Обновление доступности кнопок (блокировка при busy).
 */
    void updateControlsState();

    /**
 * @brief Включить/отключить левую панель (виджеты управления).
 * @param enabled true – включить.
 */
    void setLeftPanelEnabled(bool enabled);

    /**
 * @brief Вывод сообщения в статусную строку.
 * @param message Текст.
 * @param timeout Время отображения (мс).
 */
    void showStatusMessage(const QString& message, int timeout = 3000);

    /**
 * @brief Обновить заголовок окна (отображает состояние).
 */
    void updateWindowTitle();

    /**
 * @brief Получить битовую маску адреса для выбранного ППБ.
 * @return (1 << currentIndex).
 */
    uint16_t getSelectedAddress() const;




private:
    Ui::TesterWindow *ui;                     ///< Главный UI (из ui-файла)
    PPBController* m_controller;               ///< Указатель на контроллер
    pult* m_pultWindow;                        ///< Окно пульта (если открыто)
    bool m_displayAsCodes;                     ///< Текущий режим отображения (коды/физика)
    uint8_t m_currentPPBIndex;                  ///< Индекс текущего ППБ
    bool m_isExiting;                           ///< Флаг завершения приложения
    QVector<StatusWidget*> m_statusWidgets;     ///< Виджеты состояния для каждой вкладки

    QStackedWidget* m_stackedAkip = nullptr;    ///< Ви
    GrattenControlWidget* m_grattenWidget = nullptr;
    IAkipController* m_signalGenerator = nullptr; // не владеет
};

#endif // TESTERWINDOW_H
