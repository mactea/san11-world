#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <functional>
#include <vector>
#include "research_progress.h"
#include "trade_native_ui.h"

bool researchExchangeConfirm(int price, int balance, const uint8_t* record) {
    wchar_t name[16] = {};
    MultiByteToWideChar(950, 0, (const char*)record + 4, -1, name, 16);
    return tradenative::show({L"技巧兌換特技培養", L"確認後開始培養並扣除技巧值；返回不扣除。",
        {L"項目", L"內容"}, {{0,{L"特技", name}}, {1,{L"增加培養", L"本次1人"}},
        {2,{L"消耗技巧", std::to_wstring(price)}}, {3,{L"剩餘技巧", std::to_wstring(balance-price)}}}},
        true, true) == 0;
}

void researchExchangeRemaining(void* out, const char* format, int count, int price) {
    if (price > 0) tradenative::assign(out, L"可兌換1次（"+std::to_wstring(price)+L"技巧）");
    else ((void(__cdecl*)(void*, const char*, int))0x471d10)(out, format, count);
}
