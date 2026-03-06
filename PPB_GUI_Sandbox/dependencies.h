// dependencies.h
/**
 * @file dependencies.h
 * @brief Условное подключение зависимостей от монолитного проекта.
 *
 * В зависимости от макроса USE_REAL_DEPS подключаются либо реальные заголовки
 * (core, analyzer), либо заглушки из SUBModules/STUBS.h. Это позволяет
 * компилировать GUI отдельно от монолита в режиме разработки.
 *
 * @def USE_REAL_DEPS
 * Если определён, используются реальные заголовки монолита.
 * Если не определён – заглушки.
 */
#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

// Режим продакшена: используем реальные заголовки из монолита
#ifdef USE_REAL_DEPS
// Предполагается, что пути настроены в include_directories
#include "core/logging/logging_unified.h"
#include "core/logentry.h"
#include "core/logging/loguimanager.h"
#include "core/communication/ppbcommunication.h"
#include "core/utilits/dataconverter.h"
#include "analyzer/packetanalyzer_interface.h"
#include "analyzer/analyzer_factory.h"
// Добавьте другие необходимые заголовки

// Режим разработки: используем заглушки
#else
//Система логирования
#include "../common/logging/logmacros.h"
#include "../common/logging/logconfig.h"
#include "../common/logging/logdistributor.h"
#include "../common/logging/logentry.h"
#include "../common/logging/logwrapper.h"
#include "dataconverter.h"

#endif

#endif // DEPENDENCIES_H
