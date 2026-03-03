#ifndef CONTROLLER_DEPENDENCIES_H
#define CONTROLLER_DEPENDENCIES_H

// Общие заголовки (из common/include)

#include "ppbprotocol.h"
#include "icontroller.h"
#include "packetanalyzer_interface.h"
#include "../../AKIP-manager_module/iakipcontroller.h"
// Выбор логирования: реальное или заглушка
#ifdef USE_REAL_DEPS
#include "logging_unified.h"   // из монолита
#else

#include "logmacros.h"
#include "logconfig.h"
#include "logdistributor.h"
#include "logentry.h"
#include "logwrapper.h"
#include "dataconverter.h"
#endif

#endif // CONTROLLER_DEPENDENCIES_H
