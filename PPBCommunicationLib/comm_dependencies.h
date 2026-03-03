#ifndef COMM_DEPENDENCIES_H
#define COMM_DEPENDENCIES_H

#ifdef USE_REAL_DEPS
// Реальные зависимости (пути будут добавлены в CMake)
#include "core/logging/logging_unified.h"
#include "core/communication/ppbprotocol.h"
#else
//

//Система логирования
#include "../common/logging/logmacros.h"
#include "../common/logging/logconfig.h"
#include "../common/logging/logdistributor.h"
#include "../common/logging/logentry.h"
#include "../common/logging/logwrapper.h"

#include "ppbprotocol.h"
#endif

#endif // COMM_DEPENDENCIES_H
