#include "../src/trade_ship_ui.cpp"
#include <cstdio>
#include <stdexcept>
#include <map>
static int checks=0;
static void check(bool ok,const char* msg) {if(!ok)throw std::runtime_error(msg);++checks;printf("PASS %s\n",msg);}
static uint8_t sea[0xf4]={},land[0xf4]={},ordinary[0xf4]={};
static bool active=true;
static int identity(const uint8_t* u){return !active?0:u==sea?2:u==land?1:0;}
static int nativeCalls=0;
static std::string last;
static int __cdecl originalText(void*,const uint8_t*,int){nativeCalls++;return 77;}
static void __fastcall assign(void* out,void*,const char* s){last=s;*(int*)((uint8_t*)out+8)=(int)last.size();}
static void __fastcall label(void*,void*,const char* s){last=s;}
struct Widget {uint8_t raw[32]={};shipui::Layout layout={};bool visible=true;std::string text;};
static std::map<int,Widget> sources,widgets;
static uint8_t* root=nullptr;
static void __fastcall title(void*,void*,int id,const char* s){if(id==0x18b8)last=s;else widgets[id].text=s;}
static uint8_t* __fastcall child(void*,void*,int id){
    auto i=widgets.find(id);if(i!=widgets.end())return i->second.raw;
    auto s=sources.find(id);return s==sources.end()?nullptr:s->second.raw;
}
static POINT panelOrigin={};
static void __fastcall position(void* obj,void*,POINT* pt){
    *pt=panelOrigin;if(obj!=root){pt->x+=*(int*)((uint8_t*)obj+0xc);pt->y+=*(int*)((uint8_t*)obj+0x10);}
}
static const shipui::Layout* layout(int id){return &sources.at(id).layout;}
static uint8_t* __fastcall create(void*,void*,int id,const shipui::Layout* l,void*){
    Widget& w=widgets[id];w.layout=*l;
    *(int*)(w.raw+0xc)=l->x;*(int*)(w.raw+0x10)=l->y;*(int*)(w.raw+0x14)=l->w;*(int*)(w.raw+0x18)=l->h;
    return w.raw;
}
static void source(int id,int type,int x,int y,int width,int height,int image=-1){
    Widget& w=sources[id];w.layout={143,0,(int16_t)id,(int16_t)type,-1,(int16_t)image,9,(int16_t)x,(int16_t)y,(int16_t)width,(int16_t)height,80};
    *(int*)(w.raw+0xc)=x;*(int*)(w.raw+0x10)=y;*(int*)(w.raw+0x14)=width;*(int*)(w.raw+0x18)=height;
}
static int mouseCalls=0,opens=0;
static int __fastcall mouse(uint8_t*,void*,void*,uint32_t,POINT*){++mouseCalls;return 77;}
static bool busy=false;
static int visibility=1,showCalls=0;
static void show(uint8_t* obj,int visible){
    if(obj==root){visibility=visible;++showCalls;return;}
    for(auto& w:widgets)if(w.second.raw==obj)w.second.visible=visible!=0;
}
static bool read(const uint8_t* u,TradeShipUiDetails& out){
    if(busy)return false;out={u==sea?L"sea":L"land",u==sea?L"silk 3":L"grain 2",L"A → B",L"3 / 60 格"};return true;
}
static void open(const uint8_t*){check(shipui::modal && !shipui::pending && !visibility,"detail modal hides and suppresses underlying properties window");++opens;}
static void invokeBridge(void* hook,void* widget,void* panel,const char* s) {
    __asm {push esi
        mov esi,panel
        mov ecx,widget
        push s
        call hook
        pop esi}
}
int main(){try {
    shipui::identity=identity;
    shipui::typeName=shipui::encode(L"商隊");shipui::shipName=shipui::encode(L"大型海船");
    shipui::childNative=(shipui::ChildFn)child;shipui::posNative=(shipui::PosFn)position;
    shipui::readDetails=read;shipui::openDetails=open;shipui::downNative=mouse;shipui::upNative=mouse;
    shipui::showNative=show;
    shipui::createNative=(shipui::CreateFn)create;shipui::layoutNative=layout;
    source(0x1ad2,2,16,15,42,22,393);source(0x1ad3,6,21,18,32,16);
    source(0x1ad4,2,60,15,90,22,411);source(0x1ad5,6,65,18,80,16);
    source(0x1b22,2,431,147,58,22,393);source(0x1b23,6,444,150,32,16);
    source(0x1b24,2,433,173,54,54,366);source(0x1b25,0,436,176,48,48,639);
    source(0x1b26,2,431,231,58,22,411);source(0x1b27,6,436,234,48,16);
    shipui::textNative=originalText;shipui::assignNative=(shipui::AssignFn)assign;
    shipui::labelNative=(shipui::LabelFn)label;shipui::setLabelNative=(shipui::SetLabelFn)title;
    uint8_t out[32]={},panel[0x4000]={};root=panel;
    check(shipui::textHook(out,sea,3)==4 && last==shipui::typeName,"text hook returns native string length and generic caravan type");
    check(shipui::textHook(out,land,1)==4 && last==shipui::typeName,"unit name property also uses generic caravan label");
    check(shipui::textHook(out,sea,26)==8 && last==shipui::shipName,"sea equipment text identifies large merchant ship");
    check(shipui::textHook(out,sea,52)==77 && shipui::textHook(out,land,26)==77 && nativeCalls==2,"cargo quantities and ordinary vessels delegate unchanged");
    *(const uint8_t**)(panel+0x1a4)=sea;
    invokeBridge(shipui::landBridge,out,panel,"original");check(last==shipui::typeName,"land label bridge preserves thiscall stack and panel context");
    invokeBridge(shipui::seaBridge,out,panel,"original");check(last==shipui::shipName,"sea label bridge preserves thiscall stack and panel context");
    *(const uint8_t**)(panel+0x18c)=sea;shipui::titleLabel(panel,nullptr,0x18b8,"original");
    check(last==shipui::typeName,"unit properties title identifies registered ship");
    check(widgets.empty(),"title callback does not touch transport controls before native initialization");
    *(uintptr_t*)(panel+0x784)=0xffffffffu;*(uint32_t*)(panel+0x788)=0xffffffffu;
    shipui::panelDraw(panel,nullptr,nullptr);
    check(widgets.empty() && !shipui::pending,"early native drawing cannot query the uninitialized inventory hash table");
    *(uintptr_t*)(panel+0x784)=0x1234;*(uint32_t*)(panel+0x788)=19;
    shipui::panelDraw(panel,nullptr,nullptr);
    check(widgets.size()==17 && widgets[shipui::SHIP].layout.image==393 && widgets[shipui::SHIP+2].layout.image==366,
        "native factory reuses original vessel borders and creates only the added cells");
    check(widgets[shipui::SHIP].layout.x==497 && widgets[shipui::SHIP].layout.y==147 &&
        widgets[shipui::SHIP].layout.id==-1 && widgets[shipui::SHIP].layout.parent==-1,
        "sea vessel occupies the vacant tile beside tower ship with safe custom geometry");
    check(widgets[shipui::INFO+2].layout.w==207 && widgets[shipui::INFO+3].text==shipui::encode(L"A → B"),
        "native stat cells carry route and fit in the original left-hand gap");
    shipui::transportOffset=0x2d28;
    check(!shipui::transportReady(panel),"expanded UI does not read the old embedded inventory offset");
    *(uintptr_t*)(panel+0x2d28+0x7c)=0x1234;*(uint32_t*)(panel+0x2d28+0x80)=19;
    check(shipui::transportReady(panel),"expanded officer pool inventory uses its relocated native offset");
    shipui::transportOffset=0x708;
    shipui::panelDraw(panel,nullptr,nullptr);
    check(shipui::pending && shipui::cachedInfo.cargo==L"silk 3" && shipui::cachedKind==2,"sea properties copy cargo and vessel identity");
    check(shipui::textHook(out,land,3)==4 && last==shipui::typeName,"registered land caravan has the same generic type");
    *(const uint8_t**)(panel+0x18c)=land;shipui::titleLabel(panel,nullptr,0x18b8,"transport");
    check(last==shipui::typeName,"properties keep generic caravan title for land");
    shipui::pending=false;shipui::panelDraw(panel,nullptr,nullptr);
    check(!shipui::pending && shipui::cachedInfo.cargo==L"grain 2" && shipui::cachedKind==1,"switching to land replaces sea cargo and clears ship icon");
    check(!widgets[shipui::SHIP].visible && widgets[shipui::INFO].visible,"land caravan retains information cells and hides sea vessel tile");
    busy=true;shipui::panelDraw(panel,nullptr,nullptr);busy=false;
    check(shipui::cachedInfo.cargo!=L"grain 2" && shipui::cachedInfo.cargo!=L"silk 3","busy reader never presents another unit's cargo");
    POINT pt={140,190},outside={20,20};
    check(shipui::panelDown(panel,nullptr,nullptr,0,&pt)==1,"caravan summary accepts native mouse down");
    shipui::panelUp(panel,nullptr,nullptr,0,&pt);
    check(opens==1 && !shipui::modal && visibility==1 && showCalls==2,"completed click restores parent visibility and input after native details");
    shipui::panelDown(panel,nullptr,nullptr,0,&pt);shipui::panelUp(panel,nullptr,nullptr,0,&outside);
    check(opens==1,"dragging outside cancels opening the cargo dialog");
    shipui::panelDown(panel,nullptr,nullptr,0,&pt);*(const uint8_t**)(panel+0x18c)=sea;
    shipui::panelUp(panel,nullptr,nullptr,0,&pt);check(opens==1,"unit switch between down and up cancels opening");
    shipui::panelDown(panel,nullptr,nullptr,0,&pt);*(int*)(sea+0xc)=1;
    shipui::panelUp(panel,nullptr,nullptr,0,&pt);check(opens==1,"reused unit leader cancels a stale click");
    check(shipui::panelDown(panel,nullptr,nullptr,0,&outside)==77 && mouseCalls==1,"outside clicks delegate to the original native handler");
    panelOrigin={200,70};POINT local=pt;
    shipui::panelDown(panel,nullptr,nullptr,0,&local);shipui::panelUp(panel,nullptr,nullptr,0,&local);
    check(opens==2,"native dialog-relative click coordinates match the screen-space card");panelOrigin={};
    POINT shipPoint={510,190};
    check(shipui::hit(panel,&shipPoint),"sea vessel tile opens trade details");
    *(const uint8_t**)(panel+0x18c)=land;
    check(!shipui::hit(panel,&shipPoint),"hidden land vessel tile cannot accept clicks");
    check(shipui::fit(L"一二三四五六七八九十一二三四").find(L"...")!=std::wstring::npos,"long cargo text clips to native cell width with a visible ellipsis");
    *(const uint8_t**)(panel+0x1a4)=land;invokeBridge(shipui::landBridge,out,panel,"transport");
    check(last==shipui::typeName,"map details identify generic caravan");
    invokeBridge(shipui::seaBridge,out,panel,"walkboat");
    check(last=="walkboat","land caravan does not relabel carried boats as a large merchant ship");
    check(shipui::textHook(out,ordinary,3)==77,"ordinary transport type delegates unchanged");
    *(const uint8_t**)(panel+0x18c)=ordinary;shipui::titleLabel(panel,nullptr,0x18b8,"ordinary");
    check(last=="ordinary","properties restore ordinary title after a caravan");
    shipui::panelDraw(panel,nullptr,nullptr);
    check(!widgets[shipui::INFO].visible && !widgets[shipui::SHIP].visible,"ordinary transport hides all added native controls");
    *(const uint8_t**)(panel+0x1a4)=sea;*(const uint8_t**)(panel+0x18c)=sea;
    active=false;invokeBridge(shipui::seaBridge,out,panel,"ordinary");check(last=="ordinary","released or reused unit loses merchant label immediately");
    shipui::titleLabel(panel,nullptr,0x18b8,"ordinary");check(last=="ordinary","ordinary title restores on reused dialog");
    shipui::pending=true;shipui::panelDraw(panel,nullptr,nullptr);check(!shipui::pending,"ordinary unit clears stale caravan card");
    shipui::pending=true;tradeShipUiReset();check(!shipui::pending && !shipui::texture,"device reset clears pending card and resources");
    printf("PASS %d checks\n",checks);return 0;
}catch(const std::exception& e){printf("FAIL %s\n",e.what());return 1;}}
