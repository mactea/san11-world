#pragma once
#include "forces.h"

// Native research storage and prerequisites remain unchanged; gates apply per force.
bool researchProgressInstall(const wchar_t* dir, const wchar_t* section, Forces* forces,
                             bool write, TraceLogFn log);

bool researchExchangeConfirm(int price, int balance, const uint8_t* record);
void researchExchangeRemaining(void* out, const char* format, int count, int price);
