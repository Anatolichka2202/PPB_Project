#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QTimer>

const QString ipgratten = "192.168.1.66";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_controller(nullptr)
    , m_currentType(Unknown)
{
    ui->setupUi(this);
    m_akipPage = ui->stackedWidget->widget(0);
    m_grattenPage = nullptr;

    // Инициализация состояния UI
    setChannelControlsEnabled(false);
    updateConnectionStatus();
    logMessage("Программа запущена");

    // Запускаем автоопределение после короткой задержки
    QTimer::singleShot(100, this, &MainWindow::checkAvailableDevices);
}

MainWindow::~MainWindow()
{
    if (m_controller) {
        m_controller->closeDevice();
        delete m_controller;
    }
    delete ui;
}

// ==================== Логирование и вспомогательные методы ====================

void MainWindow::logMessage(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->txtLog->append(QString("[%1] %2").arg(timestamp).arg(msg));
}

void MainWindow::updateLastOpTimeLabel(int channel, qint64 elapsedMs)
{
    QString text = QString("Время последней операции: %1 мс").arg(elapsedMs);
    if (channel == 1)
        ui->lblLastOpTimeA->setText(text);
    else if (channel == 2)
        ui->lblLastOpTimeB->setText(text);
}

void MainWindow::setChannelControlsEnabled(bool enabled)
{
    ui->grpChannelA->setEnabled(enabled);
    ui->grpChannelB->setEnabled(enabled);
    ui->grpDelays->setEnabled(enabled);
}

void MainWindow::updateConnectionStatus()
{
    bool avail = m_controller ? m_controller->isAvailable() : false;
    if (avail) {
        ui->lblStatus->setText("🟢 Подключено");
        ui->btnConnect->setEnabled(false);
        ui->btnDisconnect->setEnabled(true);
    } else {
        ui->lblStatus->setText("🔴 Отключено");
        ui->btnConnect->setEnabled(true);
        ui->btnDisconnect->setEnabled(false);
    }
}

// ==================== Слоты подключения ====================

void MainWindow::on_btnConnect_clicked()
{
    if (m_controller) {
        logMessage("Уже подключено");
        return;
    }

    // Если автоопределение не сработало, предлагаем выбор вручную
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Выбор устройства",
                                                              "Подключиться к АКИП-3417 (USB)?\n"
                                                              "(Нажмите No для подключения к Gratten GA1483 по LAN)",
                                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply == QMessageBox::Yes) {
        switchToDevice(AKIP);
    } else if (reply == QMessageBox::No) {
        switchToDevice(GRATTEN);
    }
}

void MainWindow::on_btnDisconnect_clicked()
{
    if (m_controller) {
        m_controller->closeDevice();
        delete m_controller;
        m_controller = nullptr;
        m_currentType = Unknown;
        ui->lblIdn->setText("—");
        ui->lblDeviceType->setText("—");
        setChannelControlsEnabled(false);
        updateConnectionStatus();
        logMessage("Устройство отключено");
    }
}

void MainWindow::on_btnIdn_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    QString idn = m_controller->getIdentity();
    qint64 elapsed = m_timer.elapsed();
    ui->lblIdn->setText(idn);
    logMessage(QString("*IDN? запрошен, время ответа: %1 мс, ответ: %2").arg(elapsed).arg(idn));
}

// ==================== Канал A ====================

void MainWindow::on_btnSetFreqA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    double freq = ui->editFreqA->text().toDouble();
    if (freq <= 0) {
        logMessage("Некорректная частота для канала A");
        return;
    }
    m_timer.start();
    bool ok = m_controller->setFrequency(1, freq);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: частота установлена в %1 Гц за %2 мс").arg(freq).arg(elapsed));
    } else {
        logMessage("Ошибка установки частоты канала A");
    }
}

void MainWindow::on_btnQueryFreqA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    double freq = m_controller->queryFrequency(1);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->editFreqA->setText(QString::number(freq));
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: запрос частоты, ответ: %1 Гц, время: %2 мс").arg(freq).arg(elapsed));
    } else {
        logMessage("Ошибка запроса частоты канала A");
    }
}

void MainWindow::on_btnSetOutputA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    bool enable = ui->chkOutputA->isChecked();
    m_timer.start();
    bool ok = m_controller->setOutput(1, enable);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: выход %1 за %2 мс").arg(enable ? "ON" : "OFF").arg(elapsed));
    } else {
        logMessage("Ошибка установки выхода канала A");
    }
}

void MainWindow::on_btnQueryOutputA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    bool on = m_controller->queryOutput(1);
    qint64 elapsed = m_timer.elapsed();
    ui->chkOutputA->setChecked(on);
    updateLastOpTimeLabel(1, elapsed);
    logMessage(QString("Канал A: запрос выхода, ответ: %1, время: %2 мс").arg(on ? "ON" : "OFF").arg(elapsed));
}

void MainWindow::on_btnSetAmplA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    double ampl = ui->editAmplA->text().toDouble();
    if (ampl <= 0) {
        logMessage("Некорректная амплитуда для канала A");
        return;
    }
    QString unit = ui->cmbAmplUnitA->currentText();
    m_timer.start();
    bool ok = m_controller->setAmplitude(1, ampl, unit);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: амплитуда установлена в %1 %2 за %3 мс").arg(ampl).arg(unit).arg(elapsed));
    } else {
        logMessage("Ошибка установки амплитуды канала A");
    }
}

void MainWindow::on_btnQueryAmplA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    double ampl = m_controller->queryAmplitude(1);
    qint64 elapsed = m_timer.elapsed();
    if (ampl > 0) {
        ui->editAmplA->setText(QString::number(ampl));
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: запрос амплитуды, ответ: %1, время: %2 мс").arg(ampl).arg(elapsed));
    } else {
        logMessage("Ошибка запроса амплитуды канала A");
    }
}

void MainWindow::on_btnSetWaveA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    QString wave = ui->cmbWaveformA->currentText();
    m_timer.start();
    bool ok = m_controller->setWaveform(1, wave);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: форма сигнала %1 установлена за %2 мс").arg(wave).arg(elapsed));
    } else {
        logMessage("Ошибка установки формы сигнала канала A");
    }
}

void MainWindow::on_btnQueryWaveA_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    QString wave = m_controller->queryWaveform(1);
    qint64 elapsed = m_timer.elapsed();
    if (!wave.isEmpty()) {
        int idx = ui->cmbWaveformA->findText(wave, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveformA->setCurrentIndex(idx);
        updateLastOpTimeLabel(1, elapsed);
        logMessage(QString("Канал A: запрос формы сигнала, ответ: %1, время: %2 мс").arg(wave).arg(elapsed));
    } else {
        logMessage("Ошибка запроса формы сигнала канала A");
    }
}

// ==================== Канал B (аналогично) ====================

void MainWindow::on_btnSetFreqB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    double freq = ui->editFreqB->text().toDouble();
    if (freq <= 0) return;
    m_timer.start();
    bool ok = m_controller->setFrequency(2, freq);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: частота установлена в %1 Гц за %2 мс").arg(freq).arg(elapsed));
    } else {
        logMessage("Ошибка установки частоты канала B");
    }
}

void MainWindow::on_btnQueryFreqB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    double freq = m_controller->queryFrequency(2);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->editFreqA->setText(QString::number(freq));
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: запрос частоты, ответ: %1 Гц, время: %2 мс").arg(freq).arg(elapsed));
    } else {
        logMessage("Ошибка запроса частоты канала B");
    }
}

void MainWindow::on_btnSetOutputB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    bool enable = ui->chkOutputA->isChecked();
    m_timer.start();
    bool ok = m_controller->setOutput(2, enable);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: выход %1 за %2 мс").arg(enable ? "ON" : "OFF").arg(elapsed));
    } else {
        logMessage("Ошибка установки выхода канала B");
    }
}

void MainWindow::on_btnQueryOutputB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    bool on = m_controller->queryOutput(2);
    qint64 elapsed = m_timer.elapsed();
    ui->chkOutputA->setChecked(on);
    updateLastOpTimeLabel(2, elapsed);
    logMessage(QString("Канал B: запрос выхода, ответ: %1, время: %2 мс").arg(on ? "ON" : "OFF").arg(elapsed));
}


void MainWindow::on_btnSetAmplB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    double ampl = ui->editAmplA->text().toDouble();
    if (ampl <= 0) {
        logMessage("Некорректная амплитуда для канала B");
        return;
    }
    QString unit = ui->cmbAmplUnitA->currentText();
    m_timer.start();
    bool ok = m_controller->setAmplitude(2, ampl, unit);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: амплитуда установлена в %1 %2 за %3 мс").arg(ampl).arg(unit).arg(elapsed));
    } else {
        logMessage("Ошибка установки амплитуды канала B");
    }
}

void MainWindow::on_btnQueryAmplB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    double ampl = m_controller->queryAmplitude(2);
    qint64 elapsed = m_timer.elapsed();
    if (ampl > 0) {
        ui->editAmplA->setText(QString::number(ampl));
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: запрос амплитуды, ответ: %1, время: %2 мс").arg(ampl).arg(elapsed));
    } else {
        logMessage("Ошибка запроса амплитуды канала B");
    }
}

void MainWindow::on_btnSetWaveB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    QString wave = ui->cmbWaveformA->currentText();
    m_timer.start();
    bool ok = m_controller->setWaveform(2, wave);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: форма сигнала %1 установлена за %2 мс").arg(wave).arg(elapsed));
    } else {
        logMessage("Ошибка установки формы сигнала канала B");
    }
}

void MainWindow::on_btnQueryWaveB_clicked()
{
    if (m_currentType == GRATTEN) {
        logMessage("Канал B не поддерживается для Gratten");
        return;
    }
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    QString wave = m_controller->queryWaveform(2);
    qint64 elapsed = m_timer.elapsed();
    if (!wave.isEmpty()) {
        int idx = ui->cmbWaveformA->findText(wave, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveformA->setCurrentIndex(idx);
        updateLastOpTimeLabel(2, elapsed);
        logMessage(QString("Канал B: запрос формы сигнала, ответ: %1, время: %2 мс").arg(wave).arg(elapsed));
    } else {
        logMessage("Ошибка запроса формы сигнала канала B");
    }
}
// ==================== Тестирование задержек ====================

void MainWindow::on_btnTestIdn_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    m_timer.start();
    QString idn = m_controller->getIdentity();
    qint64 elapsed = m_timer.elapsed();
    ui->lblTestIdnTime->setText(QString::number(elapsed));
    logMessage(QString("[ТЕСТ] *IDN? время ответа: %1 мс, ответ: %2").arg(elapsed).arg(idn));
}

void MainWindow::on_btnTestSetFreq_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    int channel = ui->cmbTestChannel->currentIndex() + 1;
    double freq = ui->editTestFreq->text().toDouble();
    if (freq <= 0) {
        logMessage("[ТЕСТ] Некорректная частота");
        return;
    }
    m_timer.start();
    bool ok = m_controller->setFrequency(channel, freq);
    qint64 elapsed = m_timer.elapsed();
    if (ok) {
        ui->lblTestSetFreqTime->setText(QString::number(elapsed));
        logMessage(QString("[ТЕСТ] Установка частоты канал %1 за %2 мс").arg(channel).arg(elapsed));
    } else {
        logMessage("[ТЕСТ] Ошибка отправки команды");
    }
}

void MainWindow::on_btnTestQueryFreq_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    int channel = ui->cmbTestChannel->currentIndex() + 1;
    m_timer.start();
    double freq = m_controller->queryFrequency(channel);
    qint64 elapsed = m_timer.elapsed();
    if (freq > 0) {
        ui->lblTestQueryFreqTime->setText(QString::number(elapsed));
        logMessage(QString("[ТЕСТ] Запрос частоты канал %1: время ответа %2 мс, значение %3 Гц")
                       .arg(channel).arg(elapsed).arg(freq));
    } else {
        logMessage("[ТЕСТ] Ошибка запроса");
    }
}

void MainWindow::on_btnTestSeriesIdn_clicked()
{
    if (!m_controller || !m_controller->isAvailable()) return;
    const int count = 10;
    QList<qint64> times;
    logMessage(QString("[ТЕСТ] Серия %1 запросов *IDN? ...").arg(count));
    for (int i = 0; i < count; ++i) {
        m_timer.start();
        QString idn = m_controller->getIdentity();
        qint64 elapsed = m_timer.elapsed();
        if (!idn.isEmpty()) {
            times.append(elapsed);
        } else {
            logMessage(QString("[ТЕСТ] Ошибка в запросе #%1").arg(i+1));
        }
    }
    if (times.isEmpty()) {
        logMessage("[ТЕСТ] Нет успешных запросов");
        return;
    }
    qint64 sum = 0, min = times[0], max = times[0];
    for (qint64 t : times) {
        sum += t;
        if (t < min) min = t;
        if (t > max) max = t;
    }
    double avg = double(sum) / times.size();
    logMessage(QString("[ТЕСТ] Статистика *IDN? (%1 запросов): мин=%2 мс, макс=%3 мс, сред= %4 мс")
                   .arg(times.size()).arg(min).arg(max).arg(avg, 0, 'f', 2));
}

// ==================== Лог ====================

void MainWindow::on_btnLogClear_clicked()
{
    ui->txtLog->clear();
}

void MainWindow::on_btnLogSave_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить лог", "akip_log.txt", "Text files (*.txt)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << ui->txtLog->toPlainText();
        file.close();
        logMessage(QString("Лог сохранён в %1").arg(fileName));
    } else {
        logMessage("Ошибка сохранения лога");
    }
}

// ==================== Слоты от контроллера ====================

void MainWindow::onControllerAvailabilityChanged(bool available)
{
    updateConnectionStatus();
    logMessage(QString("Доступность устройства изменилась: %1").arg(available ? "доступно" : "недоступно"));
}

void MainWindow::onControllerError(const QString &error)
{
    logMessage("ОШИБКА: " + error);
}

void MainWindow::onControllerFrequencyChanged(int channel, double freq)
{
    if (channel == 1)
        ui->editFreqA->setText(QString::number(freq));
    else if (channel == 2)
        ui->editFreqB->setText(QString::number(freq));
    logMessage(QString("Сигнал: частота канала %1 изменена на %2 Гц").arg(channel).arg(freq));
}

void MainWindow::onControllerOutputChanged(int channel, bool enabled)
{
    if (channel == 1)
        ui->chkOutputA->setChecked(enabled);
    else if (channel == 2)
        ui->chkOutputB->setChecked(enabled);
    logMessage(QString("Сигнал: выход канала %1 изменён на %2").arg(channel).arg(enabled ? "ON" : "OFF"));
}

void MainWindow::onControllerAmplitudeChanged(int channel, double amplitude)
{
    if (channel == 1)
        ui->editAmplA->setText(QString::number(amplitude));
    else if (channel == 2)
        ui->editAmplB->setText(QString::number(amplitude));
    logMessage(QString("Сигнал: амплитуда канала %1 изменена на %2").arg(channel).arg(amplitude));
}

void MainWindow::onControllerWaveformChanged(int channel, const QString &waveform)
{
    if (channel == 1) {
        int idx = ui->cmbWaveformA->findText(waveform, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveformA->setCurrentIndex(idx);
    } else if (channel == 2) {
        int idx = ui->cmbWaveformB->findText(waveform, Qt::MatchFixedString);
        if (idx >= 0) ui->cmbWaveformB->setCurrentIndex(idx);
    }
    logMessage(QString("Сигнал: форма сигнала канала %1 изменена на %2").arg(channel).arg(waveform));
}

// ==================== Автоопределение и выбор устройства ====================

void MainWindow::checkAvailableDevices()
{
    logMessage("Поиск доступных устройств...");
    bool akipOk = false;
    QString akipIdn;
    bool grattenOk = false;
    QString grattenIdn;

    // --- Проверка АКИП (USB) ---
    {
        AkipFacade testAkip;
        if (testAkip.openDevice(0)) {
            akipIdn = testAkip.getIdentity();
            akipOk = !akipIdn.isEmpty();
            testAkip.closeDevice();
        }
    }

    // --- Проверка Граттен (LAN, IP по умолчанию) ---
    {
        GrattenGa1483Controller testGratten;
        testGratten.setConnectionParameters(ipgratten, 5025);
        if (testGratten.openDevice()) {
            grattenIdn = testGratten.getIdentity();
            grattenOk = !grattenIdn.isEmpty();
            testGratten.closeDevice();
        }
    }

    // Анализ результатов
    if (akipOk && grattenOk) {
        logMessage("Найдены оба устройства. Требуется выбор.");
        showDeviceSelectionDialog(akipIdn, grattenIdn);
    }
    else if (akipOk) {
        logMessage("Найден только АКИП. Автоподключение.");
        switchToDevice(AKIP);
    }
    else if (grattenOk) {
        logMessage("Найден только Gratten. Автоподключение.");
        switchToDevice(GRATTEN);
    }
    else {
        logMessage("Ни одно устройство не найдено. Подключитесь вручную.");
        QMessageBox::information(this, "Поиск устройств",
                                 "Не удалось обнаружить ни АКИП (USB), ни Gratten (LAN).\n"
                                 "Проверьте подключение и нажмите кнопку Подключиться для выбора.");
    }
}

void MainWindow::showDeviceSelectionDialog(const QString &akipIdn, const QString &grattenIdn)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Выбор устройства");
    msgBox.setText("Обнаружено несколько устройств. Выберите, к какому подключиться:");
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *btnAkip = msgBox.addButton(tr("АКИП-3417 (USB)"), QMessageBox::AcceptRole);
    QPushButton *btnGratten = msgBox.addButton(tr("Gratten GA1483 (LAN)"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);

    msgBox.exec();

    if (msgBox.clickedButton() == btnAkip) {
        switchToDevice(AKIP);
    } else if (msgBox.clickedButton() == btnGratten) {
        switchToDevice(GRATTEN);
    } else {
        logMessage("Выбор отменён.");
    }
}

void MainWindow::switchToDevice(DeviceType type)
{
    // Удаляем старый контроллер
    if (m_controller) {
        m_controller->closeDevice();
        delete m_controller;
        m_controller = nullptr;
    }

    // Создаём новый
    switch (type) {
    case AKIP:
        m_controller = new AkipFacade(this);
        break;
    case GRATTEN:
        m_controller = new GrattenGa1483Controller(this);
        // Устанавливаем параметры подключения
        static_cast<GrattenGa1483Controller*>(m_controller)->setConnectionParameters(ipgratten, 5025);
        break;
    default:
        return;
    }

    // Подключаем сигналы
    connect(m_controller, &IAkipController::availabilityChanged,
            this, &MainWindow::onControllerAvailabilityChanged);
    connect(m_controller, &IAkipController::errorOccurred,
            this, &MainWindow::onControllerError);
    connect(m_controller, &IAkipController::frequencyChanged,
            this, &MainWindow::onControllerFrequencyChanged);
    connect(m_controller, &IAkipController::outputChanged,
            this, &MainWindow::onControllerOutputChanged);
    connect(m_controller, &IAkipController::amplitudeChanged,
            this, &MainWindow::onControllerAmplitudeChanged);
    connect(m_controller, &IAkipController::waveformChanged,
            this, &MainWindow::onControllerWaveformChanged);

    // Пытаемся открыть
    if (m_controller->openDevice()) {
        m_currentType = type;
        QString idn = m_controller->getIdentity();
        ui->lblIdn->setText(idn);
        logMessage(QString("Подключено устройство: %1").arg(idn));

        setupForDeviceType(type);

        ui->lblDeviceType->setText(type == AKIP ? "АКИП-3417" : "Gratten GA1483");
    } else {
        logMessage("Ошибка открытия выбранного устройства");
        delete m_controller;
        m_controller = nullptr;
        m_currentType = Unknown;
    }
    updateConnectionStatus();
}

void MainWindow::setupForDeviceType(DeviceType type)
{
    // Удаляем предыдущую страницу Gratten, если она была
    if (m_grattenPage) {
        ui->stackedWidget->removeWidget(m_grattenPage);
        delete m_grattenPage;
        m_grattenPage = nullptr;
    }

    if (type == AKIP) {
        ui->stackedWidget->setCurrentWidget(m_akipPage);
        setChannelControlsEnabled(true);
        ui->grpChannelA->setEnabled(true);
        ui->grpChannelB->setEnabled(true);
    } else if (type == GRATTEN) {
        m_grattenPage = new GrattenControlWidget(m_controller, this);
        int idx = ui->stackedWidget->addWidget(m_grattenPage);
        ui->stackedWidget->setCurrentIndex(idx);

        setChannelControlsEnabled(true);   // включает группу задержек
        ui->grpChannelA->setEnabled(false);
        ui->grpChannelB->setEnabled(false);
    } else { // Unknown
        setChannelControlsEnabled(false);
    }
}
