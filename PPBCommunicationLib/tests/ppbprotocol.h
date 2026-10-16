#pragma once

// Use the shared protocol source of truth. The former test-local copy
// had conflicting FU command IDs and could silently mask wire regressions.
#include "../../common/include/ppbprotocol.h"
