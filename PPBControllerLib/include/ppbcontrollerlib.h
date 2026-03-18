#ifndef PPBCONTROLLERLIB_H
#define PPBCONTROLLERLIB_H

#include "controller_dependencies.h"
#include <QTimer>
#include <QMap>

class PPBController : public IController {
    Q_OBJECT
public:
    explicit PPBController(ICommunication* communication, PacketAnalyzerInterface* analyzer, QObject *parent = nullptr);
    ~PPBController();

    // === Управление подключением ===
    void connectToPPB(uint16_t address, const QString& ip, quint16 port) override;
    void disconnect() override;
    void setBridgeAddress(const QString &ip, quint16 port) override;

    // === Основные команды ===
    void requestStatus(uint16_t address) override;
    void sendTC(uint16_t address) override;
    void resetPPB(uint16_t address, const TCDataPayload& payload) override; // deprecated

    // === Команды ФУ ===
    void setFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle) override;
    void setFUTransmit(uint16_t address) override;

    // === Тестовые последовательности ===
    void startPRBS_M2S(uint16_t address) override;
    void startPRBS_S2M(uint16_t address) override;

    // === Информационные команды ===
    void requestVersion(uint16_t address) override;
    void requestVolume(uint16_t address) override;
    void requestChecksum(uint16_t address) override;
    void sendProgram(uint16_t address) override;
    void sendClean(uint16_t address) override;
    void requestDroppedPackets(uint16_t address) override;
    void requestBER_T(uint16_t address) override;
    void requestBER_F(uint16_t address) override;
    void requestFabricNumber(uint16_t address) override;

    // === Анализ пакетов ===
    void analize() override;
    void saveReceivedPackets(const QVector<DataPacket>& packets) override;
    void saveSentPackets(const QVector<DataPacket>& packets) override;

    // === Автоопрос ===
    void startAutoPoll(int intervalMs = 5000) override;
    void stopAutoPoll() override;
    bool isAutoPollEnabled() const override;

    // === Состояние и занятость ===
    PPBState connectionState() const override;
    bool isBusy() const override;
    void setCurrentAddress(uint16_t address) override;

    // === Получение реального состояния ППБ ===
    PPBFullState getFullState(uint8_t ppbIndex) const override;

    // === Работа с настройками пользователя ===
    const PpbUserSettings& getUserSettings(uint8_t ppbIndex) const override;
    void setPowerSetting(uint8_t ppbIndex, int channel, float watts) override;
    void setFuBlockedSetting(uint8_t ppbIndex, bool blocked) override;
    void setRebootRequestedSetting(uint8_t ppbIndex, bool reboot) override;
    void setResetErrorsSetting(uint8_t ppbIndex, bool reset) override;
    void setPowerEnabledSetting(uint8_t ppbIndex, bool enabled) override;
    void resetSettingsToReal(uint8_t ppbIndex) override;

    // === Низкоуровневый доступ ===
    void setCommunication(ICommunication* communication) override;
    void exucuteCommand(TechCommand tech, uint16_t address) override;

    // === AKIP (генератор) ===
    void setAkipInterface(IAkipController* akip);
    void setAkipFrequency(int channel, double freqHz);
    void setAkipAmplitude(int channel, double value, const QString &unit);
    void setAkipOutput(int channel, bool enable);
    void setAkipWaveform(int channel, const QString &wave);
    void setAkipDutyCycle(int channel, double percent);


    void runFullTest(uint16_t address) override;
private slots:
    // Приватные слоты для обработки сигналов от коммуникации и анализатора
    void onStatusReceived(uint16_t address, uint32_t mask, const QVector<QByteArray>& data);
    void onConnectionStateChanged(PPBState state);
    void onCommandProgress(int current, int total, TechCommand command);
    void onCommandCompleted(bool success, const QString& message, TechCommand command);
    void onErrorOccurred(const QString& error);
    void onAutoPollTimeout();
    void onBusyChanged(bool busy);
    void onSentPacketsSaved(const QVector<DataPacket>& packets);
    void onReceivedPacketsSaved(const QVector<DataPacket>& packets);
    void onClearPacketDataRequested();
    void onAnalyzerAnalysisStarted();
    void onAnalyzerAnalysisProgress(int percent);
    void onAnalyzerAnalysisComplete(const QString& summary);
    void onAnalyzerDetailedResultsReady(const QVariantMap& results);
    void onCommandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void onGroupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);

signals:
    // Дополнительные сигналы, специфичные для PPBController
    void fullStateUpdated(uint8_t ppbIndex);
    void userSettingsChanged(uint8_t ppbIndex);
    void commandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void commandCompleted(bool success, const QString& message, TechCommand command);
    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);
    void akipAvailabilityChanged(bool available);
    void autoPollToggled(bool checked);

private:
    void connectCommunicationSignals();
    void processStatusData(uint16_t address, uint32_t mask, const QVector<QByteArray>& data);
    QString commandToName(TechCommand command) const;
    void showAnalysisResults(const QString& summary, const QVariantMap& details);
    void showPacketsTable(const QString& title, const QVector<DataPacket>& packets);
    int addressToIndex(uint16_t address) const;

    ICommunication* m_communication;
    PacketAnalyzerInterface* m_packetAnalyzer;
    QTimer* m_autoPollTimer;
    bool m_autoPollEnabled;

    // Реальное состояние всех ППБ (индекс 0..15)
    PPBFullState m_realStates[16];
    // Настройки пользователя для всех ППБ
    PpbUserSettings m_userSettings[16];
    // Флаг, были ли настройки инициализированы реальными значениями
    bool m_settingsInitialized[16] = {false};

    uint16_t m_currentAddress;
    bool busy;

    QVector<DataPacket> m_lastSentPackets;
    QVector<DataPacket> m_lastReceivedPackets;

    IAkipController* m_akip; // не владеет
};

#endif // PPBCONTROLLERLIB_H
