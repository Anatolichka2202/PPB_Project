// ============================================================================
// УНИФИЦИРОВАННЫЕ МАКРОСЫ ЛОГИРОВАНИЯ
// ============================================================================
//
// Данный файл предоставляет единый интерфейс для логирования во всём проекте.
// Все макросы автоматически добавляют информацию об источнике (имя файла и строку),
// что упрощает отладку и анализ логов.
//
// ==================== КАТЕГОРИИ ЛОГИРОВАНИЯ ================================
// Категории определены в logcategories.h. Основные группы:
//   UI_*        – для отображения в пользовательском интерфейсе
//   TECH_*      – для технической отладки (мне)
//   OP_*        – для операционных отчётов (сессии, измерения)
//   GENERAL, SYSTEM – общие системные сообщения
//   CONFIG      – для логирования работы самой системы логирования
//
// ==================== БАЗОВЫЕ МАКРОСЫ ======================================
// Принимают категорию (строку) и сообщение. Используются, когда нужно явно
// указать категорию, не подходящую под стандартные алиасы.
//
//   LOG_DEBUG(cat, msg)     – отладочная информация (уровень DEBUG)
//   LOG_INFO(cat, msg)      – информационное сообщение (уровень INFO)
//   LOG_WARN(cat, msg)      – предупреждение (уровень WARNING)
//   LOG_ERROR(cat, msg)     – ошибка (уровень ERROR)
//   LOG_CRITICAL(cat, msg)  – критическая ошибка (уровень CRITICAL)
//
// Пример: LOG_ERROR(LogCategory::TECH_PROTOCOL, "Неверный формат пакета");
//
// ==================== СТРУКТУРИРОВАННЫЕ ДАННЫЕ =============================
// Эти макросы принимают готовые структуры (TableData, CardData, ProgressData)
// и создают структурированную запись, которая будет отображаться в UI особым
// образом (таблицы, карточки, прогресс-бары).
//
//   LOG_TABLE(table)        – вывод таблицы (для статуса ППБ и т.п.)
//   LOG_CARD(card)          – вывод карточки (сводка результатов)
//   LOG_PROGRESS(progress)  – вывод прогресс-бара (ход выполнения операции)
//
// Пример: LOG_TABLE(statusTable);
//
// ==================== УДОБНЫЕ АЛИАСЫ ПО КАТЕГОРИЯМ =========================
// Рекомендуется использовать именно их в большинстве случаев.
// Они автоматически подставляют соответствующую категорию и уровень.
//
// ------------------------ UI  ------------------------------------
//   LOG_UI_STATUS(msg)      – обновление состояния интерфейса (например,
//                             "Подключение установлено")
//   LOG_UI_OPERATION(msg)   – действия пользователя (нажатие кнопок,
//                             выбор ППБ)
//   LOG_UI_RESULT(msg)      – успешное завершение операции (не от ППБ)
//   LOG_UI_ALERT(msg)       – ошибки и предупреждения, важные для всех
//   LOG_UI_DATA(msg)        – сырые данные (например, UDP-пакеты) – полезно
//                             при отладке, но может быть много
//   LOG_PPB_RESULT(msg)     – результаты выполнения команд ППБ (версия,
//                             контрольная сумма, BER) – эти сообщения будут
//                             подсвечиваться в UI
//
// -------------------- Technical (детали) -------------------------
//   LOG_TECH_STATE(msg)     – изменение состояния модуля (например,
//                             "Переход в состояние Ready")
//   LOG_TECH_NETWORK(msg)   – сетевая активность (отправка/приём пакетов)
//   LOG_TECH_PROTOCOL(msg)  – детали протокола (парсинг, построение пакетов)
//   LOG_TECH_DEBUG(msg)     – отладочная информация, которая не нужна в
//                             обычном режиме
//   LOG_TECH_THREAD(msg)    – информация о потоках (создание, завершение)
//
// -------------------- Operational (для отчёта) -------------------------
//   LOG_OP_SESSION(msg)     – начало/конец сессии работы
//   LOG_OP_OPERATION(msg)   – выполняемая операция (например, "Запуск теста")
//   LOG_OP_MEASUREMENT(msg) – результаты измерений
//   LOG_OP_SUMMARY(msg)     – сводка по сессии
//
// -------------------- Общие -------------------------------------------------
//   LOG_SYSTEM(msg)         – системные события (инициализация, завершение)
//   LOG_GENERAL(msg)        – всё остальное, что не подходит под другие
//                             категории
//   LOG_CONFIG(msg)         – логирование конфигурации (обычно используется
//                             только внутри LogConfig)
//
// ============================================================================
// ВАЖНО: Старые макросы (LOG_CAT_INFO, LOG_DEBUG и т.д.) больше не
// поддерживаются. Весь новый код должен использовать макросы из этого файла.
// ============================================================================

#ifndef LOGMACROS_H
#define LOGMACROS_H

#include "logwrapper.h"
#include "logcategories.h"

// ==================== БАЗОВЫЕ МАКРОСЫ ====================
// Используются для логирования с произвольной категорией.
// Автоматически добавляют имя файла и номер строки.

#define LOG_DEBUG(cat, msg)    LogWrapper::log(LogLevel::LOG_DEBUG, cat, msg, __FILE__, __LINE__)
#define LOG_INFO(cat, msg)     LogWrapper::log(LogLevel::LOG_INFO, cat, msg, __FILE__, __LINE__)
#define LOG_WARN(cat, msg)     LogWrapper::log(LogLevel::LOG_WARNING, cat, msg, __FILE__, __LINE__)
#define LOG_ERROR(cat, msg)    LogWrapper::log(LogLevel::LOG_ERROR, cat, msg, __FILE__, __LINE__)
#define LOG_CRITICAL(cat, msg) LogWrapper::log(LogLevel::LOG_CRITICAL, cat, msg, __FILE__, __LINE__)

// ==================== СТРУКТУРИРОВАННЫЕ ДАННЫЕ ====================
// Для таблиц, карточек, прогресса (источник не указываем, т.к. данные уже структурированы)

#define LOG_TABLE(table)       LogWrapper::logTable(table)
#define LOG_CARD(card)         LogWrapper::logCard(card)
#define LOG_PROGRESS(progress) LogWrapper::logProgress(progress)

// ==================== УДОБНЫЕ АЛИАСЫ ПО КАТЕГОРИЯМ ====================
// Для часто используемых категорий, чтобы не писать длинные строки.

// UI (для оператора)
#define LOG_UI_STATUS(msg)     LOG_INFO(LogCategory::UI_STATUS, msg)
#define LOG_UI_CONNECTION(msg)  LOG_INFO(LogCategory::UI_CONNECTION, msg)
#define LOG_UI_OPERATION(msg)  LOG_INFO(LogCategory::UI_OPERATION, msg)
#define LOG_UI_RESULT(msg)     LOG_INFO(LogCategory::UI_RESULT, msg)
#define LOG_UI_ALERT(msg)      LOG_WARN(LogCategory::UI_ALERT, msg)
#define LOG_UI_DATA(msg)       LOG_DEBUG(LogCategory::UI_DATA, msg)
#define LOG_PPB_RESULT(msg)    LOG_INFO(LogCategory::UI_PPB_RESULT, msg)
#define LOG_UI_TABLE(table)       LogWrapper::logTable(table)
#define LOG_UI_CARD(card)       LogWrapper::logCard(card)

// Technical (для разработчика)
#define LOG_TECH_STATE(msg)    LOG_INFO(LogCategory::TECH_STATE, msg)
#define LOG_TECH_NETWORK(msg)  LOG_INFO(LogCategory::TECH_NETWORK, msg)
#define LOG_TECH_PROTOCOL(msg) LOG_DEBUG(LogCategory::TECH_PROTOCOL, msg)
#define LOG_TECH_DEBUG(msg)    LOG_DEBUG(LogCategory::TECH_DEBUG, msg)
#define LOG_TECH_THREAD(msg)   LOG_INFO(LogCategory::TECH_THREAD, msg)

// Operational (для отчётности)
#define LOG_OP_SESSION(msg)    LOG_INFO(LogCategory::OP_SESSION, msg)
#define LOG_OP_OPERATION(msg)  LOG_INFO(LogCategory::OP_OPERATION, msg)
#define LOG_OP_MEASUREMENT(msg) LOG_INFO(LogCategory::OP_MEASUREMENT, msg)
#define LOG_OP_SUMMARY(msg)    LOG_INFO(LogCategory::OP_SUMMARY, msg)

// Общие
#define LOG_SYSTEM(msg)        LOG_INFO(LogCategory::SYSTEM, msg)
#define LOG_GENERAL(msg)       LOG_INFO(LogCategory::GENERAL, msg)

// Конфигурация (для самого логгера)
#define LOG_CONFIG(msg)        LOG_INFO(LogCategory::CONFIG, msg)

#endif // LOGMACROS_H
