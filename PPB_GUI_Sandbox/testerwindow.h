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
#include <QSet>
#include <QStackedWidget>
#include "grattencontrolwidget.h"
#include "iakipcontroller.h"
#include "statuswidget.h"
#include "pult.h"
#include "ppbcontrollerlib.h"
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui { class TesterWindow; }
QT_END_NAMESPACE

class TesterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TesterWindow(PPBController* controller, QWidget *parent = nullptr);
    ~TesterWindow();

    // Установка контроллера генератора (из ApplicationManager)
    void setSignalGeneratorController(IAkipController* ctrl);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Слоты от контроллера ППБ
    void onFullStateUpdated(uint8_t ppbIndex);
    void onConnectionStateChanged(PPBState state);
    void onErrorOccurred(const QString &error);
    void onBusyChanged(bool busy);
    void onOperationProgress(int current, int total, const QString &operation);
    void onOperationCompleted(bool success, const QString &message);

    // Слоты от виджетов левой панели
   // void onConnectRequested(const QString &ip, quint16 port);
    void onDisconnectRequested();
    void onExitClicked();
    void onPultClicked();
    void onMetricsClicked();
    void onAkipPultClicked();
    void onReconnectGeneratorClicked();

    // Слоты от DataViewWidget
    void onDisplayModeChanged(bool codes);

    void onBridgePingRequested(const QString &ip, quint16 port);
    void onPpbProbeRequested(uint16_t mask);

    // Слоты для вкладок ППБ
    void onTabClicked(int index);
    void onTabBarCurrentChanged(int index);

    //пульты
    void onPultDestroyed(QObject* obj); // новый слот для очистки карты


private:
    void setupPpbTabs(int count = 16);
    void updateSelectedCountLabel();
    QList<uint16_t> getSelectedAddresses() const;
    uint16_t getSelectedAddress() const; // для обратной совместимости
    uint16_t getSelectedMask() const;
    void updateGeneratorUi();

    void updateTabSelectionStyle();

    Ui::TesterWindow *ui;
    PPBController* m_controller;
    IAkipController* m_signalGenerator = nullptr;

    // Данные для множественного выбора вкладок
    QSet<int> m_selectedTabs;
    int m_lastSelectedTab = -1;
    QVector<StatusWidget*> m_statusWidgets;

    bool m_displayAsCodes = false;
    bool m_isExiting = false;

     QMap<uint16_t, pult*> m_pultWindows; // карта открытых пультов
};

#endif // TESTERWINDOW_H
