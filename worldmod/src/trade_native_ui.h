#pragma once
#include "trade_images.h"
// Original game's list/select dialog and numeric keypad. All drawing, scrolling,
// keyboard navigation and modal lifetime belong to Koei; only the row data is ours.
// ABI: research/city_trade_menu_re.md (32-bit Traditional Chinese executable).
namespace tradenative {
struct Row { int id; std::vector<std::wstring> cells; bool enabled = true; std::wstring picture; };
struct Table {
    std::wstring title, help;
    std::vector<std::wstring> columns;
    std::vector<Row> rows;
    std::wstring picture;
};
struct ViewState { std::vector<int> order; int scroll=0,focus=-1; };
inline Table restoreView(const Table& source,const ViewState* state) {
    Table table=source;
    if(state)std::stable_sort(table.rows.begin(),table.rows.end(),[&](const Row& a,const Row& b) {
        auto rank=[&](int id) {auto it=std::find(state->order.begin(),state->order.end(),id);return it-state->order.begin();};
        return rank(a.id)<rank(b.id);
    });
    return table;
}
#ifdef TRADE_UI_TEST
inline std::function<int(const Table&,bool,bool,ViewState*,std::vector<int>*)> testShow;
inline std::function<int(const std::wstring&,int,int,int)> testNumber;
#endif
inline std::string encode(const std::wstring& s) {
    int n = WideCharToMultiByte(950, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(950, 0, s.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}
inline void* assign(void* out, const std::wstring& s) {
    std::string text = encode(s);
    ((void (__thiscall*)(void*, const char*))0x4719f0)(out, text.c_str());
    return out;
}
struct Context {
    const Table* table;
    bool confirm = false;
    bool pictures = false;
    void* list = nullptr;
    void* dialogVt[0x1e8 / 4];
    void* listVt[0x200 / 4];
    std::vector<int> columns, widths;
    struct Group { const char* name; int* columns; int count; } group;
};
static Context* current = nullptr; // only one modal at a time; cleared before the next step
inline std::wstring pictureForRow(const Table& table, int64_t key) {
    return key>=0 && key<(int64_t)table.rows.size() && !table.rows[(size_t)key].picture.empty()
        ? table.rows[(size_t)key].picture : table.picture;
}
inline void __fastcall drawList(uint8_t* list, void*, void* draw) {
    ((void (__thiscall*)(void*,void*))0x4e2740)(list,draw);
    if(!current || !current->pictures) return;
    int hovered=*(int*)(list+0xe0); // Native focus/hover row, including keyboard navigation.
    int64_t key=-1;
    if(hovered>=0 && hovered<(int)current->table->rows.size()) {
        auto row=((int64_t* (__thiscall*)(void*,int))0x4e5070)(list,hovered);
        if(row) key=*row; // stable key survives native column sorting
    }
    POINT pos;
    ((void* (__thiscall*)(void*,void*))0x475880)(list,&pos);
    tradeImageQueue(pictureForRow(*current->table,key),pos.x,pos.y-88);
}
inline int __fastcall count(void*, void*) { return (int)current->columns.size(); }
inline void* __fastcall header(void*, void*, void* out, int col) {
    return assign(out, current->table->columns[col]);
}
inline int __fastcall width(void*, void*, int col) { return current->widths[col]; }
inline int __fastcall sortMode(void*, void*, int) { return current->columns.size()>2 ? 1 : 0; }
inline void* __fastcall cellText(void*, void*, void* out, int row, int, int col) {
    const auto& rows = current->table->rows;
    return assign(out, row >= 0 && row < (int)rows.size() && col >= 0 && col < (int)rows[row].cells.size()
        ? rows[row].cells[col] : L"");
}
inline int __fastcall sortValue(void*, void*, int row, int, int col, int) {
    const auto& rows = current->table->rows;
    if (row < 0 || row >= (int)rows.size() || col < 0 || col >= (int)rows[row].cells.size()) return INT_MIN;
    const std::wstring& s = rows[row].cells[col];
    wchar_t* end = nullptr;
    long v = wcstol(s.c_str(), &end, 10);
    if (end != s.c_str() && !*end) return v;
    int rank = 0;
    for (auto& r : rows) if (col < (int)r.cells.size() && r.cells[col] < s) ++rank;
    return rank;
}
inline void* __fastcall group(void*, void*, int index) { return index == 0 ? &current->group : nullptr; }
inline void* __fastcall makeList(uint8_t* dlg, void*, int id, uint8_t* layout) {
    struct Init { int x,y,w,h,flags; void* parent; int* columns; int count,fixed,max,style,reserved; };
    Init init = { *(int16_t*)(layout+0xe), *(int16_t*)(layout+0x10), *(int16_t*)(layout+0x12),
        *(int16_t*)(layout+0x14), 3, dlg, current->columns.data(), (int)current->columns.size(),
        *(int*)(dlg+0x194), current->confirm ? 0 : *(int*)(dlg+0x190), 0xf3, 0 };
    void* list = ((void* (__cdecl*)(int))0x6450b0)(0x1ac);
    if (!list) return nullptr;
    ((void (__thiscall*)(void*))0x4e47c0)(list);
    *(void***)list = current->listVt;
    if (!((int (__thiscall*)(void*, void*))0x4e5770)(list, &init) ||
        !((int (__thiscall*)(void*, int, void*, int))0x4dc880)(dlg, id, list, 1)) {
        ((void (__thiscall*)(void*, int))current->listVt[0])(list, 1);
        return nullptr;
    }
    current->list = list;
    return list;
}
// Returns the stable row ID, not the visual index (which changes when sorted).
// select=false is a native read-only 一覽, with only 返回.
inline int show(const Table& source, bool select = true, bool confirm = false,ViewState* state=nullptr,std::vector<int>* selections=nullptr) {
    Table table=restoreView(source,state);
#ifdef TRADE_UI_TEST
    if(testShow)return testShow(table,select,confirm,state,selections);
#endif
    Context ctx = {}; ctx.table = &table; ctx.confirm = confirm;
    ctx.pictures=!table.picture.empty();
    for(auto& row:table.rows) ctx.pictures |= !row.picture.empty();
    for (int c = 0; c < (int)table.columns.size(); ++c) {
        ctx.columns.push_back(c);
        auto units = [](const std::wstring& s) { int n=0; for (wchar_t ch:s) n += ch < 128 ? 1 : 2; return n; };
        int n = units(table.columns[c]);
        for (auto& r : table.rows) if (c < (int)r.cells.size()) n = std::max(n, units(r.cells[c]));
        ctx.widths.push_back(std::max(64, std::min(540, n * 9 + 28)));
    }
    int total = 0; for (int w : ctx.widths) total += w;
    if (total < 740 && !ctx.widths.empty()) for (int& w : ctx.widths) w += (740-total)/(int)ctx.widths.size();
    std::string category = encode(L"一覽"), title = encode(table.title), help = encode(table.help);
    ctx.group = { category.c_str(), ctx.columns.data(), (int)ctx.columns.size() };
    memcpy(ctx.dialogVt, (void*)0x85b9e0, sizeof ctx.dialogVt);
    memcpy(ctx.listVt, (void*)0x8344f8, sizeof ctx.listVt);
    ctx.dialogVt[0x1d8/4] = (void*)makeList; ctx.dialogVt[0x1e4/4] = (void*)group;
    ctx.listVt[0x19c/4] = (void*)count; ctx.listVt[0x1a0/4] = (void*)header;
    ctx.listVt[0x1a4/4] = (void*)width; ctx.listVt[0x1e0/4] = (void*)cellText;
    ctx.listVt[0x1ac/4] = (void*)sortMode;
    ctx.listVt[0x1fc/4] = (void*)sortValue;
    if(ctx.pictures) ctx.listVt[0x1c/4]=(void*)drawList;
    current = &ctx;
    alignas(8) uint8_t dlg[0x2cc] = {}, params[0x144] = {};
    ((void (__thiscall*)(void*))0x64cf20)(dlg);
    *(void***)dlg = ctx.dialogVt;
    ((void (__thiscall*)(void*))0x4ae240)(params);
    *(const char**)(params+0xc) = title.c_str();
    *(int*)(params+0x18) = select && !confirm && !selections ? 1 : 0;
    *(int*)(params+0x1c) = selections ? std::max(2,(int)table.rows.size()) : confirm ? 2 : select ? 1 : 0;
    ((void (__thiscall*)(void*,const char*))0x4719f0)(params+0x24,help.c_str());
    ((void (__thiscall*)(void*,const char*))0x4719f0)(params+0xb0,help.c_str());
    int created = ((int (__thiscall*)(void*,void*))0x64e1d0)(dlg, params);
    int result = -1;
    if (created && ctx.list) {
        for (int i=0; i<(int)table.rows.size(); ++i) {
            ((int (__thiscall*)(void*,int,int,int))0x4e6340)(ctx.list, INT_MAX, i, 0);
            // Native row record +8 controls availability; +c is selection.
            void* row = ((void* (__thiscall*)(void*,int))0x4e5070)(ctx.list,i);
            if (row) {
                *(int*)((uint8_t*)row+8) = table.rows[i].enabled ? 1 : 0;
                if(selections && table.rows[i].enabled)
                    *(int*)((uint8_t*)row+0xc)=std::find(selections->begin(),selections->end(),table.rows[i].id)!=selections->end();
            }
        }
        for (int id : {0xb5d,0xb5e}) ((void (__thiscall*)(void*,int,int))0x4d7b00)(dlg,id,0);
        ((void (__thiscall*)(void*))0x64d0d0)(dlg);
        if(ctx.pictures) {
            auto list=(uint8_t*)ctx.list;
            POINT pos={*(int*)(list+0xc),*(int*)(list+0x10)+88};
            SIZE size={*(int*)(list+0x14),*(int*)(list+0x18)-88};
            ((void (__thiscall*)(void*,void*))0x4733d0)(list,&pos);
            ((void (__thiscall*)(void*,void*))0x473410)(list,&size);
            ((void (__thiscall*)(void*))0x4e4ae0)(list); // recalculate native scrolling / hit rectangles
        }
        if(state && !table.rows.empty()) {
            *(int*)((uint8_t*)ctx.list+0xdc)=std::max(0,std::min(state->scroll,(int)table.rows.size()-1));
            for(int i=0;i<(int)table.rows.size();++i)if(table.rows[i].id==state->focus)*(int*)((uint8_t*)ctx.list+0xe0)=i;
        }
        int answer = ((int (__thiscall*)(void*))0x64cb20)(dlg);
        if(state) {
            state->order.clear();state->scroll=*(int*)((uint8_t*)ctx.list+0xdc);
            int focus=*(int*)((uint8_t*)ctx.list+0xe0);
            for(int i=0;i<(int)table.rows.size();++i) {
                auto row=((int64_t* (__thiscall*)(void*,int))0x4e5070)(ctx.list,i);
                if(row && *row>=0 && *row<(int64_t)table.rows.size()) {
                    int id=table.rows[(size_t)*row].id;state->order.push_back(id);if(i==focus)state->focus=id;
                }
            }
        }
        if (confirm && answer == 10000) result = 0;
        else if (select && answer == 10000) {
            if(selections) {selections->clear();result=0;}
            // +dc is the scroll offset, NOT the selected row. Read native row
            // selection flags and their stable keys, including after sorting.
            for (int i=0; i<(int)table.rows.size(); ++i) {
                uint8_t* row = ((uint8_t* (__thiscall*)(void*,int))0x4e5070)(ctx.list,i);
                if (!row || !*(int*)(row+0xc)) continue;
                int64_t key=*(int64_t*)row;
                if (key>=0 && key<(int64_t)table.rows.size() && table.rows[(size_t)key].enabled) {
                    int id=table.rows[(size_t)key].id;
                    if(selections)selections->push_back(id);else {result=id;break;}
                }
            }
        }
    }
    ((void (__thiscall*)(void*))0x64cfb0)(dlg);
    current = nullptr;
    return result;
}
inline int number(const std::wstring& title, int value, int maximum, const std::wstring& picture=L"",int minimum=1) {
#ifdef TRADE_UI_TEST
    if(testNumber)return testNumber(title,value,maximum,minimum);
#endif
    std::string text = encode(title);
    struct { const char* title; int minimum, maximum, value, flags; void* callback; }
        params = { text.c_str(), minimum, maximum, std::max(minimum,std::min(value,maximum)), 0, nullptr };
    if(maximum<minimum) return -1;
    tradeImageNumber(picture);
    int result=((int (__cdecl*)(void*))0x4f2630)(&params);
    tradeImageNumber(L"");
    return result;
}
} // namespace tradenative
