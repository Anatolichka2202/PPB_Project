#ifndef LOGCATEGORIES_H
#define LOGCATEGORIES_H

#include <QString>

namespace LogCategory {
// UI – для оператора
inline const QString UI_STATUS     = QStringLiteral("UI_STATUS");
inline const QString UI_OPERATION  = QStringLiteral("UI_OPERATION");
inline const QString UI_RESULT     = QStringLiteral("UI_RESULT");
inline const QString UI_ALERT      = QStringLiteral("UI_ALERT");
inline const QString UI_DATA       = QStringLiteral("UI_DATA");
inline const QString UI_PPB_RESULT = QStringLiteral("UI_PPB_RESULT"); // подсветка
inline const QString UI_CONNECTION = QStringLiteral("UI_CONNECTION");

// Technical – для разработчика
inline const QString TECH_STATE    = QStringLiteral("TECH_STATE");
inline const QString TECH_NETWORK  = QStringLiteral("TECH_NETWORK");
inline const QString TECH_PROTOCOL = QStringLiteral("TECH_PROTOCOL");
inline const QString TECH_DEBUG    = QStringLiteral("TECH_DEBUG");
inline const QString TECH_THREAD   = QStringLiteral("TECH_THREAD");
inline const QString TECH_PERFORMANCE = QStringLiteral("TECH_PERFORMANCE");

// Operational – для отчётности
inline const QString OP_SESSION    = QStringLiteral("OP_SESSION");
inline const QString OP_OPERATION  = QStringLiteral("OP_OPERATION");
inline const QString OP_MEASUREMENT = QStringLiteral("OP_MEASUREMENT");
inline const QString OP_SUMMARY    = QStringLiteral("OP_SUMMARY");

// Общие
inline const QString GENERAL       = QStringLiteral("GENERAL");
inline const QString SYSTEM        = QStringLiteral("SYSTEM");

// Конфигурация (для самого логгера)
inline const QString CONFIG        = QStringLiteral("CONFIG");
}

// Категории логирования, используемые во всём проекте.
// UI_* – для отображения в пользовательском интерфейсе (оператору).
// TECH_* – для технической отладки (разработчикам).
// OP_* – для операционных отчётов (сессии, измерения).
// GENERAL, SYSTEM – общие системные сообщения.
// CONFIG – для логирования самой системы конфигурации.
#endif // LOGCATEGORIES_H
