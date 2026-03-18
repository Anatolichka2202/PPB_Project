#ifndef PULT_H
#define PULT_H

#include <QWidget>
#include "ppbcontrollerlib.h"
#include <QMessageBox>
#include <QLabel>

namespace Ui {
class pult;
}

class pult : public QWidget
{
    Q_OBJECT

public:
    explicit pult(uint16_t address, PPBController* controller, QWidget *parent = nullptr);
    ~pult();

private slots:
    // Команды
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
    void on_AnalizeBttn_clicked();
    void on_FabricNumber_clicked();

    // Обратная связь от контроллера
    void onControllerErrorOccurred(const QString& error);
    void onControllerOperationCompleted(bool success, const QString& message);
    void onAnalysisStarted();
    void onAnalysisProgress(int percent);
    void onAnalysisComplete(const QString& summary, const QVariantMap& details);

    // Редактирование полей
    void onPower1Changed(const QString& text);
    void onPower2Changed(const QString& text);
    void onFuBlockedToggled(bool checked);
    void onRebootToggled(bool checked);
    void onResetErrorsToggled(bool checked);

    // Обновление от контроллера
    void onFullStateUpdated(uint8_t ppbIndex);
    void onUserSettingsChanged(uint8_t ppbIndex);  // новый слот

private:
    Ui::pult *ui;
    PPBController* m_controller;
    uint16_t m_address;
    QTimer* m_statusTimer;

    int getPpbIndex() const;
    void refreshSettings(); // принудительное обновление полей из настроек
};

#endif // PULT_H
