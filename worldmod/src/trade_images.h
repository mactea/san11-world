#pragma once
#include <string>
#include "d3dtrace.h"
struct IDirect3DDevice9;

// Original commodity files in <dataDir>/trade_goods; names, not list positions,
// identify pictures so native sorting and reordered trade data remain safe.
void tradeImagesInit(const wchar_t* dataDir, TraceLogFn log);
void tradeImageQueue(const std::wstring& name, int x, int y);
void tradeImageNumber(const std::wstring& name);
void tradeImagesDraw(IDirect3DDevice9* device);
void tradeImagesReset();
