/**
 * @file pult.h
 * @brief Окно пульта управления для конкретного ППБ.
 *
 * Предоставляет расширенный набор команд для одного ППБ (адрес фиксирован).
 * Открывается из главного окна по кнопке «Пульт».
 */
#ifndef PULT_H
#define PULT_H

#include <QWidget>
#include "ppbcontrollerlib.h"
#include <QMessageBox>
#include <QLabel>
namespace Ui {
/**
 * @class pult
 * @brief Диалоговое окно с кнопками для всех команд ППБ.
 *
 * Содержит кнопки для запроса версии, объёма, контрольной суммы,
 * запуска PRBS тестов, анализа и т.д. Работает с уже подключённым
 * контроллером и использует фиксированный адрес, переданный в конструкторе.
 */
class pult;
}

class pult : public QWidget
{
    Q_OBJECT
public:
    explicit pult(uint16_t address, PPBController* controller, QWidget *parent = nullptr);
    ~pult();

private slots:
    // Существующие слоты для кнопок команд
    void on_TSComand_clicked();
    void on_TCCommand_clicked();          // теперь будет вызывать sendTC
    void on_PRBS_S2MCommand_clicked();
    void on_PRBS_M2SCommand_clicked();
    void on_VERSComand_clicked();
    void on_VolumeComand_clicked();
    void on_ChecksumCommand_clicked();
    void on_ProgramCommand_clicked();
    void on_CleanCommand_clicked();
    void on_DropCommand_clicked();
    void on_BER_TCommand_clicked();
    void on_BER_FCommand_clicked();
    void on_AnalizeBttn_clicked();
    void on_FabricNumber_clicked();

    // Слоты для обратной связи от контроллера
    void onControllerErrorOccurred(const QString& error);
    void onControllerOperationCompleted(bool success, const QString& message);
    void onAnalysisStarted();
    void onAnalysisProgress(int percent);
    void onAnalysisComplete(const QString& summary, const QVariantMap& details);

    // **НОВЫЕ** слоты для изменения полей ввода
    void onPower1Changed(const QString& text);
    void onPower2Changed(const QString& text);
    void onFuBlockedToggled(bool checked);
    void onRebootToggled(bool checked);
    void onResetErrorsToggled(bool checked);

    // **НОВЫЙ** слот для обновления UI при изменении состояния
    void onFullStateUpdated(uint8_t ppbIndex);

private:
    Ui::pult *ui;
    PPBController* m_controller;   // указатель на контроллер (передаётся в конструктор)
    uint16_t m_address;            // адрес ППБ, для которого открыт пульт
    QTimer* m_statusTimer;

    // Вспомогательный метод для получения индекса ППБ по адресу
    int getPpbIndex() const;
};
#endif // PULT_H
