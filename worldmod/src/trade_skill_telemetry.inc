struct MerchantTelemetry {
    uint8_t* person=nullptr;uint8_t* persons=nullptr;
    int id=-1,owned=-1,force=-1,city=-1,affiliation[5]={};uint64_t generation=0;
    MerchantTelemetry(int selected,int f,int c):id(selected),force(f),city(c){
        if(!skilltelemetry::enabled() || id<0 || id>=g_cfg.PT-g_cfg.specials || !g_person0)return;
        generation=skilltelemetry::contextGeneration();persons=g_person0;person=persons+(size_t)id*0x190;
        owned=*(int*)(person+0xe8);memcpy(affiliation,person+0x94,sizeof affiliation);
    }
    bool valid(const Caravan* committed=nullptr)const{
        bool stable=person && skilltelemetry::enabled() && generation==skilltelemetry::contextGeneration() &&
            persons==g_person0 && id>=0 && id<g_cfg.PT-g_cfg.specials && person==g_person0+(size_t)id*0x190 &&
            *(int*)(person+0xe8)==owned && *(int*)(person+0xa0)>=0 && *(int*)(person+0xa0)<=3 &&
            *(void**)person && vcall(person,8) && vcall(person,0x40)==force && baseForce(city)==force;
        if(!stable)return false;
        // Formation can legitimately move this same officer into the newly
        // committed caravan after the city's discount/limit was evaluated.
        if(committed && committed->leader==id && committed->force==force && committed->origin==city && liveUnit(*committed))return true;
        return !memcmp(affiliation,person+0x94,sizeof affiliation) && *(int*)(person+0x9c)==city;
    }
};
void merchantFeeCommitted(const MerchantTelemetry& source,const Caravan& k,int destination,int ordinary,int charged,int total,bool nextLeg){
    if(charged>=ordinary || !source.valid(&k))return;
    skilltelemetry::Target target;target.force=k.force;target.city=target.base=k.origin;target.unit=k.unit;char context[320];
    sprintf_s(context,"{\"caravan_id\":%u,\"origin_city\":%d,\"destination_city\":%d,\"ship_type\":%d,\"continuation\":%s,\"total_fee_paid\":%d,\"result_scope\":\"discounted_fee_component_of_committed_payment\",\"counterfactual_evaluated\":false}",
        k.id,k.origin,destination,k.shipType,nextLeg?"true":"false",total);target.context=context;
    skilltelemetry::applied(source.person,source.owned,SKILL_MERCHANT,"trade","merchant_departure_fee_discount_committed",
        "trade_fee",ordinary,charged,0,target);
}
void merchantCapacityCommitted(const MerchantTelemetry& source,const Caravan& k,bool ai,int before,int after,int limit){
    int ordinary=ai?tradeauto::AI_CITY_LIMIT:tradeauto::PLAYER_CITY_LIMIT;
    if(before<ordinary || after!=before+1 || after>limit || !source.valid(&k))return;
    skilltelemetry::Target target;target.force=k.force;target.city=target.base=k.origin;target.unit=k.unit;char context[288];
    sprintf_s(context,"{\"caravan_id\":%u,\"ai\":%s,\"ordinary_limit\":%d,\"merchant_limit\":%d,\"result_scope\":\"extra_city_slot_used_by_committed_caravan\",\"counterfactual_evaluated\":false}",
        k.id,ai?"true":"false",ordinary,limit);target.context=context;
    skilltelemetry::applied(source.person,source.owned,SKILL_MERCHANT,"trade","merchant_extra_caravan_slot_used",
        "active_automatic_caravans",before,after,0,target);
}
void merchantSaleCommitted(const MerchantTelemetry& source,const Caravan& k,int market,int marketForce,int good,int qty,int64_t basis,
        int64_t duty,int64_t debt,int64_t bonus,int64_t net){
    if(bonus<=0 || !source.valid())return;
    skilltelemetry::Target target;target.force=marketForce;target.city=target.base=market;target.unit=k.unit;char context[400];
    sprintf_s(context,"{\"caravan_id\":%u,\"origin_city\":%d,\"good_id\":%d,\"quantity\":%d,\"cargo_basis\":%lld,\"duty\":%lld,\"legacy_debt\":%lld,\"bonus\":%lld,\"automatic\":%s,\"result_scope\":\"sale_payout_including_pending_credit\",\"counterfactual_evaluated\":false}",
        k.id,k.origin,good,qty,basis,duty,debt,bonus,k.automatic.caravan?"true":"false");target.context=context;
    skilltelemetry::applied(source.person,source.owned,SKILL_MERCHANT,"trade","caravan_sale_profit_bonus","sale_payout",net-bonus,net,0,target);
}
