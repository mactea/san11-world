// City markets may use a nearby native port owned by the market's force.
// Freight transfer is abstracted as a paid, timed handoff; ships never sail on land.
const int PORT_REACH = 20;
struct ShipAccess {
    int base=-1,lo=0,hi=0,steps=0;
    int fee() const {return steps*2;}
    int days() const {return ((steps+9)/10)*10;}
};
uint8_t* nativePort(int id) {
    if(id<g_s.C+10 || id>=g_s.C+45)return nullptr;
    auto b=baseRec(WORLD,id);
    return objValid(b) && *(int*)(b+8)==2 ? b : nullptr;
}
bool waterNear(int lo,int hi) {
    for(int x=lo-3;x<=lo+3;++x)for(int y=hi-3;y<=hi+3;++y) {
        int d=hexDist(lo,hi,x,y);
        if(d>=1 && d<=3 && waterHex(cell(x,y)))return true;
    }
    return false;
}
// A bounded land connection, not straight-line distance across rivers or cliffs.
int portLandSteps(int lo,int hi,int toLo,int toHi) {
    if(hexDist(lo,hi,toLo,toHi)>PORT_REACH)return -1;
    const int side=PORT_REACH*2+1;
    int distances[side*side];std::fill_n(distances,side*side,-1);
    std::vector<uint32_t> queue={pack(lo,hi)};
    distances[PORT_REACH*side+PORT_REACH]=0;
    for(size_t i=0;i<queue.size();++i) {
        int x=(int16_t)queue[i],y=(int16_t)(queue[i]>>16);
        int step=distances[(x-lo+PORT_REACH)*side+y-hi+PORT_REACH];
        if(x==toLo && y==toHi)return step;
        if(step==PORT_REACH)continue;
        for(int nx=x-1;nx<=x+1;++nx)for(int ny=y-1;ny<=y+1;++ny) {
            if(hexDist(x,y,nx,ny)!=1 || abs(nx-lo)>PORT_REACH || abs(ny-hi)>PORT_REACH)continue;
            auto h=cell(nx,ny);if(!h)continue;
            int index=(nx-lo+PORT_REACH)*side+ny-hi+PORT_REACH;
            if(distances[index]>=0)continue;
            int t=*(uint32_t*)(h+4)&31;
            bool end=nx==toLo && ny==toHi;
            if(!end && !(t<=5 || (t>=9 && t<=14) || t==19 || (t==16 && hexDist(lo,hi,nx,ny)<=2)))continue;
            distances[index]=step+1;queue.push_back(pack(nx,ny));
        }
    }
    return -1;
}
ShipAccess shipAccessAt(int city,int anchor) {
    if(city<0 || city>=g_data.C)return {};
    int lo,hi;cityPos(city,&lo,&hi);
    if(anchor==city)return waterNear(lo,hi) ? ShipAccess{city,lo,hi,0} : ShipAccess{};
    auto b=nativePort(anchor);int owner=baseForce(city);
    if(!b || owner<0 || vcall(b,0x40)!=owner)return {};
    auto p=posOf(b);
    if(!waterNear(p[0],p[1]))return {};
    int steps=portLandSteps(lo,hi,p[0],p[1]);
    return steps>=0 ? ShipAccess{anchor,p[0],p[1],steps} : ShipAccess{};
}
ShipAccess shipAccess(int city) {
    if(city<0 || city>=g_data.C)return {};
    auto direct=shipAccessAt(city,city);if(direct.base>=0)return direct;
    int lo,hi;cityPos(city,&lo,&hi);int owner=baseForce(city);
    if(owner<0)return {};
    ShipAccess best;
    for(int id=g_s.C+10;id<g_s.C+45;++id) {
        auto b=nativePort(id);if(!b || vcall(b,0x40)!=owner)continue;
        auto p=posOf(b);if(hexDist(lo,hi,p[0],p[1])>PORT_REACH)continue;
        auto a=shipAccessAt(city,id);
        if(a.base>=0 && (best.base<0 || a.steps<best.steps))best=a;
    }
    return best;
}
bool shipPort(int city) {return shipAccess(city).base>=0;}
