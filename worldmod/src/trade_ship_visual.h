#pragma once
#include <stdint.h>
#include "d3dtrace.h"

typedef bool (*TradeShipIdentityFn)(int unitId);
// Optional visual only: missing assets or incompatible native code retain native boats.
bool tradeShipVisualInstall(const wchar_t* dataDir, TradeShipIdentityFn identity, TraceLogFn log);
void tradeShipVisualReset();
