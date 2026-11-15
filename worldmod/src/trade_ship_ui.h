#pragma once
#include <stdint.h>
#include <string>
#include "d3dtrace.h"
struct IDirect3DDevice9;
// 0 = ordinary unit, 1 = registered land caravan, 2 = registered sea caravan.
using TradeShipUiIdentity = int (*)(const uint8_t* unit);
struct TradeShipUiDetails { std::wstring summary, cargo, route, load; };
// Copies owned text without waiting for the trade logic lock. No runtime pointers escape.
using TradeShipUiRead = bool (*)(const uint8_t* unit, TradeShipUiDetails& out);
using TradeShipUiOpen = void (*)(const uint8_t* unit);
bool tradeShipUiInstall(const wchar_t* dataDir, TradeShipUiIdentity identity,
    TradeShipUiRead read, TradeShipUiOpen open, TraceLogFn log);
void tradeShipUiDraw(IDirect3DDevice9* device);
void tradeShipUiReset();
