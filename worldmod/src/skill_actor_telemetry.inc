static bool telemetryActor(const void* person, skilltelemetry::Actor& out) {
    out=skilltelemetry::Actor{};
    if(!g_persons || !g_persons->PERSONARR)return false;
    uintptr_t first=(uintptr_t)g_persons->PERSONARR, address=(uintptr_t)person;
    if(address<first || address-first>=(size_t)g_persons->PT*0x190 || (address-first)%0x190)return false;
    auto p=(const uint8_t*)person;
    out.person=(int)((address-first)/0x190);
    memcpy(out.name,p+4,5);memcpy(out.name+strlen(out.name),p+9,5);
    // Current location also encodes units, gates and ports. The city column
    // contains a city only; affiliation at+9c is not the actor's current city.
    int location=*(const int*)(p+0x98),cities=g_bases?g_bases->C:42;
    if(location>=0 && location<cities)out.city=location;
    int corps=*(const int*)(p+0x94);
    if(g_forces && g_forces->CORPSARR && g_forces->FORCEARR && corps>=0 && corps<g_forces->F) {
        int force=*(int*)(g_forces->CORPSARR+corps*0x50+4);
        if(force>=0 && force<g_forces->F) {
            out.force=force;int ruler=*(int*)(g_forces->FORCEARR+force*0x12c+4);
            if(ruler>=0 && ruler<g_persons->PT) {
                out.ruler=ruler;
                auto ruler=g_persons->PERSONARR+(size_t)out.ruler*0x190;
                memcpy(out.rulerName,ruler+4,5);memcpy(out.rulerName+strlen(out.rulerName),ruler+9,5);
            }
        }
    }
    auto w=(const int*)0x7201958;
    if(w[2]>0 && w[2]<10000 && w[3]>=1 && w[3]<=12 && w[4]>=1 && w[4]<=30 && w[0x5c/4]>=0) {
        int64_t days=(((int64_t(w[2])-1)*12+w[3]-1)*3+w[0x5c/4])*10+w[4]-1;
        if(days<9999LL*360) {
            out.year=int(days/360+1);out.month=int(days%360/30+1);out.day=int(days%30+1);
        }
    }
    return true;
}
