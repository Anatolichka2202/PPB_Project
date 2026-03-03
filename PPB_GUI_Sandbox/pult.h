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
    void on_TSComand_clicked();
    void on_TCCommand_clicked();
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
    void onControllerLogMessage(const QString& message);
    void onControllerErrorOccurred(const QString& error);
    void onControllerOperationCompleted(bool success, const QString& message);

    void onAnalysisStarted();
    void onAnalysisProgress(int percent);
    void onAnalysisComplete(const QString& summary, const QVariantMap& details);


    void on_AnalizeBttn_clicked();

    void on_FabricNumber_clicked();

private:
    Ui::pult *ui;
    PPBController* m_controller;
    uint16_t m_address;
    QTimer* m_statusTimer;
};

#endif // PULT_H
