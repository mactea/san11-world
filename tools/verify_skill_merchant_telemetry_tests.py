import json


def verify_merchant(rows):
    positive=rows('merchant_effects_test.csv');edges=rows('merchant_effects_edges_test.csv')
    fee='merchant_departure_fee_discount_committed';slot='merchant_extra_caravan_slot_used';sale='caravan_sale_profit_bonus'
    expected=[]
    for owned in (147,249):
        expected.extend([(fee,14162,owned,0,0),(slot,14162,owned,0,0),(sale,14162,owned,0,0),(fee,14162,owned,0,0),
                         (fee,14162,owned,0,-1),(slot,14162,owned,0,-1)])
    expected.extend([(fee,14162,147,0,0),(slot,14162,147,0,0),(sale,14162,147,0,0),
                     (fee,600,249,0,-1),(slot,600,249,0,-1),(fee,14162,147,1,0),(slot,14162,147,1,0)])
    assert len(positive)==len(expected)==19 and len(edges)==2
    for r,want in zip(positive,expected):
        assert (r['event'],int(r['person_id']),int(r['owned_skill']),int(r['force_id']),int(r['actor_city']))==want
    assert [r['event'] for r in edges]==[fee,slot]
    for r in positive+edges:
        assert r['stage']=='applied' and r['scene']=='trade' and r['effect_skill']=='147'
        assert r['target_unit']=='1' and r['actor_unit']=='-1' and r['target_person']=='-1'
        assert not r['baseline_after'] and not r['bonus_delta'] and int(r['caller'])==0
        context=json.loads(r['context_json']);assert context['caravan_id']==12 and context['counterfactual_evaluated'] is False
        if r['event']==fee:
            continuation=context['continuation'];ordinary,actual=(30,24) if continuation else (100,80)
            assert (r['metric'],int(r['before']),int(r['after']),int(r['delta']))==('trade_fee',ordinary,actual,actual-ordinary)
            assert context==dict(caravan_id=12,origin_city=0,destination_city=0 if continuation else 1,ship_type=0,
                continuation=continuation,total_fee_paid=actual,result_scope='discounted_fee_component_of_committed_payment',
                counterfactual_evaluated=False)
        elif r['event']==slot:
            ai=r['force_id']=='1';before,after,limit=(1,2,2) if ai else (2,3,4)
            assert (r['metric'],int(r['before']),int(r['after']),int(r['delta']))==('active_automatic_caravans',before,after,1)
            assert context==dict(caravan_id=12,ai=ai,ordinary_limit=before,merchant_limit=limit,
                result_scope='extra_city_slot_used_by_committed_caravan',counterfactual_evaluated=False)
        else:
            assert r['event']==sale and (r['metric'],r['before'],r['after'],r['delta'])==('sale_payout','285','321','36')
            assert context==dict(caravan_id=12,origin_city=0,good_id=0,quantity=1,cargo_basis=105,duty=0,legacy_debt=0,
                bonus=36,automatic=True,result_scope='sale_payout_including_pending_credit',counterfactual_evaluated=False)
        market=1 if r['event']==sale else 0
        assert (r['target_city'],r['target_base'],r['target_force'])==(str(market),str(market),r['force_id'])
    return dict(merchant_effect_rows=21,merchant_effect_applied_rows=21,merchant_effect_query_rows=0)
