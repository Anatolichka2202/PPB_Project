#include "loguimanager.h"

LogUIManager::LogUIManager(QObject* parent)
    : QObject(parent), m_levelCombo(nullptr)
{
}

void LogUIManager::setupLevelComboBox(QComboBox* levelCombo)
{
    m_levelCombo = levelCombo;

    levelCombo->clear();
    levelCombo->addItem("Все", -1);          // специальное значение для "Все"
    levelCombo->addItem("DEBUG", LOG_DEBUG);
    levelCombo->addItem("INFO", LOG_INFO);
    levelCombo->addItem("WARNING", LOG_WARNING);
    levelCombo->addItem("ERROR", LOG_ERROR);
    levelCombo->setCurrentIndex(1);           // DEBUG по умолчанию (можно 0 для "Все")

    connect(levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogUIManager::onLevelChanged);

    updateUIConfig();
}

void LogUIManager::onLevelChanged(int /*index*/)
{
    updateUIConfig();
}

void LogUIManager::updateUIConfig()
{
    if (!m_levelCombo) return;

    int data = m_levelCombo->currentData().toInt();
    if (data == -1) {
        // "Все" – устанавливаем самый низкий уровень
        LogConfig::instance().setMinLevel("ui", LOG_DEBUG);
    } else {
        LogLevel level = static_cast<LogLevel>(data);
        LogConfig::instance().setMinLevel("ui", level);
    }
}

LogLevel LogUIManager::getCurrentLevel() const
{
    if (m_levelCombo) {
        return static_cast<LogLevel>(m_levelCombo->currentData().toInt());
    }
    return LOG_INFO;
}
