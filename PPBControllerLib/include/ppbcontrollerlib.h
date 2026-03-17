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

    // Реализация чистых виртуальных методов IController (все с override)
    void connectToPPB(uint16_t address, const QString& ip, quint16 port) override;
    void disconnect() override;
    void requestStatus(uint16_t address) override;
    void resetPPB(uint16_t address, const TCDataPayload& payload) override;
    void setGeneratorParameters(uint16_t address, uint32_t duration, uint8_t duty, uint32_t delay) override;
    void setFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle) override;
    void setFUTransmit(uint16_t address) override;
    void startPRBS_M2S(uint16_t address) override;
    void startPRBS_S2M(uint16_t address) override;
    void runFullTest(uint16_t address) override;
    void startAutoPoll(int intervalMs = 5000) override;
    void stopAutoPoll() override;
    void setCurrentAddress(uint16_t address) override;
    PPBState connectionState() const override;
    bool isBusy() const override;
    bool isAutoPollEnabled() const override;

    void exucuteCommand(TechCommand tech, uint16_t) override;
    void requestVersion(uint16_t address) override;
    void requestVolume(uint16_t address) override;
    void requestChecksum(uint16_t address) override;
    void sendProgram(uint16_t address) override;
    void sendClean(uint16_t address) override;
    void requestDroppedPackets(uint16_t address) override;
    void requestBER_T(uint16_t address) override;
    void requestBER_F(uint16_t address) override;
    void analize() override;
    void saveReceivedPackets(const QVector<DataPacket>& packets) override;
    void saveSentPackets(const QVector<DataPacket>& packets) override;
    void setCommunication(ICommunication* communication) override;
    void requestFabricNumber(uint16_t address) override;

    void setBridgeAddress(const QString &ip, quint16 port);

public:
    void setChannelPower(uint8_t ppbIndex, int channel, float watts);
    void setFuBlocked(uint8_t ppbIndex, bool blocked);
    void setRebootRequested(uint8_t ppbIndex, bool reboot);
    void setResetErrors(uint8_t ppbIndex, bool reset);
    void sendTC(uint16_t address);
    PPBFullState getFullState(uint8_t ppbIndex) const;

    // ====================AKIP===============================================================================
public:
    void setAkipInterface(IAkipController* akip); // новый метод
    // Методы для управления АКИП
    void setAkipFrequency(int channel, double freqHz);
    void setAkipAmplitude(int channel, double value, const QString &unit);
    void setAkipOutput(int channel, bool enable);
    void setAkipWaveform(int channel, const QString &wave);
    void setAkipDutyCycle(int channel, double percent);

    void setPowerEnabled(uint8_t ppbIndex, bool enabled);
    // Сигнал о доступности
signals:
    void akipAvailabilityChanged(bool available);
private:
    IAkipController* m_akip; // не владеет, просто указатель

    // ===================================================================================================

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
    void fullStateUpdated(uint8_t ppbIndex);
    void commandDataParsed(uint16_t address, const QVariant& data, TechCommand command);
    void commandCompleted(bool success, const QString& message, TechCommand command);
    void groupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary);

private:
    void connectCommunicationSignals();
    void processStatusData(uint16_t address, uint32_t mask, const QVector<QByteArray>& data);
    //UIChannelState parseChannelData(const QVector<QByteArray>& channelData);
    QString commandToName(TechCommand command) const;
    void showAnalysisResults(const QString& summary, const QVariantMap& details);
    void showPacketsTable(const QString& title, const QVector<DataPacket>& packets);

    ICommunication* m_communication;
    PacketAnalyzerInterface* m_packetAnalyzer;
    QTimer* m_autoPollTimer;
    bool m_autoPollEnabled;
    QMap<uint8_t, PPBFullState> m_ppbStates;
    uint16_t m_currentAddress;
    bool busy;
    QVector<DataPacket> m_lastSentPackets;
    QVector<DataPacket> m_lastReceivedPackets;


    int addressToIndex(uint16_t address) const
    {
        for (int i = 0; i < 16; ++i) {
            if (address == (1 << i)) return i;
        }
        return -1;
    }
};

#endif // PPBCONTROLLERLIB_H
