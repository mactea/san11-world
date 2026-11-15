#include "../src/trade_ship_visual.cpp"

static int visualCalls=0,customCalls=0;
static bool acceptShip=true;
static bool failCustomDraw=false;
static bool testIdentity(int id){return acceptShip && id==42;}
static int __fastcall testBind(void*,void*,void*){return 1;}
static HRESULT __fastcall testDraw(void*,void*,IDirect3DDevice9* device,void* context,void* definition,int offset,void* batches,unsigned count) {
    ++visualCalls;
    if(definition==model) {
        require(*outlineEnabled==0,"expanded outline suppressed only during merchant draw");
        ++customCalls;require(context==nullptr && offset==0 && batches==mesh.data()+160 && count==1,"custom native draw arguments");
        IDirect3DBaseTexture9* bound=nullptr;device->GetTexture(0,&bound);require(bound==texture,"merchant texture scoped to custom draw");if(bound)bound->Release();
        if(failCustomDraw)return E_FAIL;
    } else require(definition==boatModel && context==(void*)0x123,"ordinary boat keeps original native draw arguments");
    return S_OK;
}
static void visualCases(IDirect3DDevice9* device,unsigned char* code,const char* gameExe,const char* asset) {
    std::string resource=gameExe;resource=resource.substr(0,resource.find_last_of("/\\")+1)+"Media/san11pkres.bin";
    std::ifstream pack(resource,std::ios::binary);unsigned at=0,size=0;pack.seekg(16+0x8ad*8);pack.read((char*)&at,4);pack.read((char*)&size,4);
    require(!!pack && size>=1000 && size<1024*1024,"native rig fixture bounds");
    std::vector<uint8_t> rig(size);pack.seekg(at);pack.read((char*)rig.data(),size);require(!!pack,"native rig fixture read");uint8_t originalModel[0x2c]={};
    *(void**)(originalModel+0x24)=rig.data();boatModel=originalModel;
    std::string folder=asset;folder=folder.substr(0,folder.find_last_of("/\\"));assetDir=std::wstring(folder.begin(),folder.end());
    auto bytes=read(asset),tex=read((folder+"/merchant_ship.wft").c_str());
    require(validAsset(bytes,tex,rig.data()),"visual loader accepts native-compatible mesh");
    auto bad=bytes;bad.resize(900);require(!validAsset(bad,tex,rig.data()),"truncated mesh rejected");
    bad=bytes;bad[124]=1;require(!validAsset(bad,tex,rig.data()),"file-supplied GPU pointer rejected");
    bad=bytes;bad[800]^=1;require(!validAsset(bad,tex,rig.data()),"incompatible bone hierarchy rejected");
    bad=bytes;bad.back()=255;require(!validAsset(bad,tex,rig.data()),"out-of-range index rejected");
    loadNative=(Load)(code+0xa5d0);uploadNative=(Upload)(code+0xa660);releaseNative=(Release)(code+0xa8b0);
    bindNative=(Bind)testBind;drawNative=(Draw)testDraw;identityFn=testIdentity;
    unsigned outline=1;outlineEnabled=&outline;
    void* instance=(void*)INSTANCES;void* actor=(void*)ACTORS;void* batches=rig.data()+160;
    require(slot((void*)(ACTORS+1),ACTORS,0x50,4096)==-1 && slot((void*)(INSTANCES+128*0x770),INSTANCES,0x770,128)==-1,"pool bounds and alignment");
    drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);
    require(visualCalls==1 && customCalls==0 && texture==nullptr,"ordinary ship does not allocate custom resources");
    rememberActor(actor,42);bindHook(instance,actor,boatModel);
    IDirect3DTexture9* previous=nullptr;require(SUCCEEDED(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&previous,nullptr)),"sentinel texture");device->SetTexture(0,previous);
    drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);
    require(customCalls==1 && texture!=nullptr && *(void**)(originalModel+0x24)==rig.data(),"merchant selects private mesh and preserves original model");
    require(outline==1,"native outline restored after merchant draw");
    IDirect3DBaseTexture9* restored=nullptr;device->GetTexture(0,&restored);require(restored==previous,"native texture and cache restored after merchant draw");if(restored)restored->Release();
    int before=visualCalls;drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,rig.data()+164,1);require(visualCalls==before,"old translucent oars suppressed");
    acceptShip=false;drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);require(customCalls==1,"finished/reused unit identity rechecked at draw");acceptShip=true;
    rememberActor(actor,-1);bindHook(instance,actor,boatModel);require(instanceUnits[0]==0,"actor and instance reuse clears merchant identity");
    tradeShipVisualReset();require(texture==nullptr && !owner && !actorUnits[0] && !instanceUnits[0],"device reset clears resources and tags");
    rememberActor(actor,42);bindHook(instance,actor,boatModel);drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);require(customCalls==2,"resources recreated after reset");
    failCustomDraw=true;before=visualCalls;drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);
    require(visualCalls==before+2 && customCalls==3 && failed,"failed custom draw immediately restores native boat");
    require(outline==1,"native outline restored before failed-draw fallback");
    drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);require(customCalls==3,"failed renderer is not retried every frame");failCustomDraw=false;
    tradeShipVisualReset();assetDir+=L"_missing";rememberActor(actor,42);bindHook(instance,actor,boatModel);
    before=visualCalls;drawHook(instance,nullptr,device,(void*)0x123,boatModel,0,batches,2);require(visualCalls==before+1 && customCalls==3 && failed,"missing asset falls back to native draw");
    tradeShipVisualReset();device->SetTexture(0,nullptr);previous->Release();boatModel=nullptr;
    printf("PASS ship integration: identity, reuse, scoped texture, opaque pass, malformed assets, reset/reload, missing-asset fallback\n");
}
