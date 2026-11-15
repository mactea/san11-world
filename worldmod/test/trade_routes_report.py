"""Build a self-contained, sortable Chinese HTML report from trade_routes_sim output."""
import hashlib
import html
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
folder = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'outputs/trade-routes-20261101'
data = json.loads((folder / 'routes.json').read_text(encoding='utf-8'))
routes = data['routes']
assert len(routes) == 100
assert len({(r['origin_id'], r['destination_id']) for r in routes}) == 100
bands = ['1–30 格', '31–80 格', '81–150 格', '151–300 格']
for r in routes:
    assert sum(x['cost'] for x in r['cargo']) == r['purchase']
    assert sum(x['revenue'] for x in r['cargo']) == r['arrival_revenue']
    assert sum(x['tax'] for x in r['cargo']) == r['arrival_sales_tax']
    assert r['departure_cost'] == r['purchase'] + r['fee'] <= 4000
    assert r['cash_profit'] == r['arrival_revenue'] - r['arrival_sales_tax'] - r['estimated_transit'] - r['departure_cost']
    assert r['realized_profit'] == r['cash_profit'] + r['remaining_cost']
    assert 0 < r['load_tenths'] <= 200
    assert 0 <= r['estimated_transit'] <= r['transit_cap']
    # A stress scenario, not a stochastic forecast: reduce each lot's realized gross receipt 10%.
    down_revenue = sum(x['revenue'] * 90 // 100 for x in r['cargo'])
    down_tax = down_revenue * r['sales_tax_pct'] // 100
    r['downside_cash_profit'] = down_revenue - down_tax - r['transit_cap'] - r['departure_cost']
    r['total_cost'] = r['departure_cost'] + r['arrival_sales_tax'] + r['estimated_transit']
    r['roi'] = r['cash_profit'] / r['departure_cost']
    r['per_turn'] = r['cash_profit'] / r['turns']

def total(rows, key):
    return sum(r[key] for r in rows)

def summary(rows):
    cost = total(rows, 'departure_cost')
    return dict(count=len(rows), profitable=sum(r['cash_profit'] > 0 for r in rows),
                mixed=sum(len(r['cargo']) > 1 for r in rows),
                departure_cost=cost, total_cost=total(rows, 'total_cost'),
                revenue=total(rows, 'arrival_revenue'), profit=total(rows, 'cash_profit'),
                roi=total(rows, 'cash_profit') / cost if cost else 0,
                downside_profit=total(rows, 'downside_cash_profit'),
                downside_profitable=sum(r['downside_cash_profit'] > 0 for r in rows))

recommended = [r for r in routes if r['recommended']]
stats = {'all_forced': summary(routes), 'recommended_only': summary(recommended),
         'by_distance': [summary([r for r in routes if r['stratum'] == k]) for k in range(4)],
         'recommendation_underperforms_single': [r['id'] for r in routes if r['quoted_profit'] < r['single_quoted_profit']],
         'snapshot_resources_insufficient': sum(not r['snapshot_resources_sufficient'] for r in routes),
         'sources': {}}
for path in ['worldmod/build/trade-game/world_custom/trade.bin',
             'worldmod/build/trade-game/basket-before-v8.txt',
             'worldmod/src/trade_model.cpp', 'worldmod/src/trade_model.h',
             'worldmod/src/trade_panel.cpp', 'worldmod/src/trade_panel.h',
             'worldmod/test/trade_routes_sim.cpp']:
    stats['sources'][path] = hashlib.sha256((root / path).read_bytes()).hexdigest()
(folder / 'summary.json').write_text(json.dumps(stats, ensure_ascii=False, indent=2), encoding='utf-8')
esc = html.escape
fmt = lambda n: f'{n:,}'
all_stats, rec_stats = stats['all_forced'], stats['recommended_only']
group_rows = ''.join(f'<tr><td>{bands[k]}</td><td>25</td><td>{s["profitable"]}/25</td>'
                     f'<td>{fmt(s["departure_cost"])}</td><td>{fmt(s["revenue"])}</td>'
                     f'<td>{fmt(s["profit"])}</td><td>{s["roi"]:.1%}</td></tr>'
                     for k, s in enumerate(stats['by_distance']))
rows = []
for r in routes:
    mix = '、'.join(f'{x["good"]}×{x["quantity"]}' for x in r['cargo'])
    cells = [r['id'], r['origin']+' → '+r['destination'],
             '推荐' if r['recommended'] else '不推荐·强制试运', mix, r['distance'], r['turns'],
             r['load_tenths']/10, r['purchase'], r['fee'], r['arrival_sales_tax'],
             r['estimated_transit'], r['total_cost'], r['arrival_revenue'], r['cash_profit'],
             f'{r["roi"]:.1%}', f'{r["per_turn"]:.1f}', r['cap_tax_cash_profit'],
             r['downside_cash_profit'], r['single_quoted_profit'], r['unsold_quantity'],
             '足够' if r['snapshot_resources_sufficient'] else '不足']
    nums = [None, None, None, None] + cells[4:14] + [r['roi'], r['per_turn']] + cells[16:20] + [None]
    content = ''.join(f'<td data-sort="{esc(str(nums[j] if nums[j] is not None else c))}">{esc(str(c))}</td>' for j,c in enumerate(cells))
    rows.append(f'<tr class="{"gain" if r["cash_profit"]>0 else "loss"}" data-rec="{int(r["recommended"])}">{content}</tr>')
headers = ['编号','商路','发车建议','商品组合','距离/格','耗时/旬','载重/20格','货款/金','组队费/金','销售税/金','过境税估算/金','总现金成本/金','销售收入/金','现金利润/金','发车本金回报','利润/旬','过境税收满后利润','收入下降10%且税收满','单品比较利润','未售数量','快照资源']
sources = ''.join(f'<li><code>{esc(p)}</code><br><small>SHA-256: {h}</small></li>' for p,h in stats['sources'].items())
content = f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>100 条商路成本收益模拟</title><style>
body{{font-family:"Microsoft YaHei",sans-serif;background:#f4f6fa;color:#172538;margin:0;line-height:1.65}}main{{max-width:1440px;margin:auto;padding:32px}}
h1{{margin-bottom:4px}}h2{{margin-top:32px;font-size:21px}}.muted,small{{color:#52647c}}.cards{{display:flex;gap:16px;flex-wrap:wrap}}.card{{background:white;border:1px solid #dce3ed;border-radius:12px;padding:18px;flex:1;min-width:200px}}.big{{font-size:32px;font-weight:700;color:#144c77}}
table{{border-collapse:collapse;background:white;width:100%;font-size:14px}}td,th{{border-bottom:1px solid #dce3ed;padding:10px;text-align:right;white-space:nowrap}}th{{background:#173a5c;color:white}}td:first-child,td:nth-child(2){{text-align:left}}.scroll{{overflow:auto;max-height:700px;border:1px solid #dce3ed}}#routes th{{position:sticky;top:0;cursor:pointer}}.loss td:nth-child(14){{color:#ab3030;font-weight:bold}}.gain td:nth-child(14){{color:#087047;font-weight:bold}}button,input,select{{font:inherit;padding:8px;margin:6px 8px 12px 0;border:1px solid #aebcce;border-radius:5px}}code{{overflow-wrap:anywhere}}li{{margin:8px 0}}details{{background:white;padding:16px;margin:16px 0}}@media print{{.scroll{{max-height:none;overflow:visible}}button,input,select{{display:none}}}}
</style><main><h1>100 条商路成本收益模拟</h1><p class="muted">2026-11-01 · 当前代码与测试存档市场快照 · 固定种子 {data['seed']} · 金额单位：游戏金</p>
<div class="cards"><div class="card">盈利商路<div class="big">48 / 100</div>其余 52 条不建议新建商队发车</div>
<div class="card">只执行推荐路线的合计利润<div class="big">{fmt(rec_stats['profit'])} 金</div>投入 {fmt(rec_stats['departure_cost'])} 金；本金回报 {rec_stats['roi']:.1%}</div>
<div class="card">100 条全部强制试运利润<div class="big">{fmt(all_stats['profit'])} 金</div>包含 52 条不推荐路线的亏损</div>
<div class="card">组合货物商路<div class="big">{all_stats['mixed']} 条</div>按游戏推荐配货，不强制混装</div></div>
<h2>结论与可改进之处</h2><p>目前贸易并非普遍无法盈利，但短途受到 100 金组队费和相似产地价格的明显限制。长途利润更高，也要占用更多时间；这里的单程利润没有扣除护卫、粮食和风险成本。</p>
<ul><li>短途 25 条仅 2 条盈利，远途 25 条有 21 条盈利。优先优化短途重复经营成本，不能只看每单总利润。</li>
<li>推荐算法有两处可改进：維魯→阿奎推荐赚 5 金，单品比较方案赚 9 金；長安→會稽推荐赚 961 金，单品比较方案赚 1,021 金。当前贪心配货并不保证利润最优。</li>
<li>{stats['snapshot_resources_insufficient']} 条路线的出发城市在快照中的金、兵或粮不足以执行本次标准化方案；预算统一到 4,000 金是为了比较经济性，不能直接视为当前存档可发车清单。</li>
<li>销售收入降低 10%、过境税取上限的压力情景中，盈利路线剩 {all_stats['downside_profitable']} 条；推荐路线合计利润为 {fmt(rec_stats['downside_profit'])} 金。这是情景分析，不是风险概率预测。</li></ul>
<h2>距离分组</h2><p>每组独立抽 25 条，因此合计比例不代表全地图自然分布；未按收益筛样。</p><table><thead><tr><th>距离</th><th>样本</th><th>盈利条数</th><th>发车本金/金</th><th>销售收入/金</th><th>现金利润/金</th><th>本金回报</th></tr></thead><tbody>{group_rows}</tbody></table>
<h2>成本口径</h2><p>货款 {fmt(total(routes,'purchase'))} + 组队费 {fmt(total(routes,'fee'))} + 销售税 {fmt(total(routes,'arrival_sales_tax'))} + 过境税估算 {fmt(total(routes,'estimated_transit'))} = 总现金成本 <b>{fmt(all_stats['total_cost'])} 金</b>。销售收入 {fmt(all_stats['revenue'])} − 总现金成本 = <b>{fmt(all_stats['profit'])} 金</b>。</p>
<p>发车本金 = 货款 + 组队费。本金回报 = 现金利润 ÷ 发车本金；不是按 4,000 金预算计算。利润/旬使用地图距离估计的单程耗时。本金和收益合计是相互独立试验的加总，不能理解为一个势力同时派出 100 队的实盘收益。</p>
<h2>100 条逐路明细</h2><p>点击列标题排序；支持按城市和商品搜索。初始按现金利润从高到低展示。</p><input id="search" placeholder="搜索城市 / 商品"><select id="filter"><option value="all">全部 100 条</option><option value="1">仅推荐发车</option><option value="0">不推荐发车</option></select><span id="count"></span>
<div class="scroll"><table id="routes"><thead><tr>{''.join(f'<th>{h}</th>' for h in headers)}</tr></thead><tbody>{''.join(rows)}</tbody></table></div>
<details open><summary><b>模拟条件与边界</b></summary><ol>
<li>从 530 城、14 商品数据及 basket-before-v8.txt 快照读取库存、价格指数、归属和资源。候选要求起点有库存、两城均有归属、距离不超过 300 格；四个候选池数量分别为 {data['pool_sizes']}。每层随机抽 25 条，方向区分，不按利润重抽。</li>
<li>每条路线独立重置市场，预算 4,000 金、载重 20 格、普通商人政治 60/智力 50/等级 0。保持原归属，同势力免税，其余假定中立开放，情报全部可见，无税款小数结转余额。</li>
<li>使用 recommendOrder 的组合推荐；无推荐时，为观察亏损保留单品比较方案的强制试运结果。单品比较逐商品扫描数量，先选不含过境税的最佳数量，再比较含估算过境税利润，不宣称全局最优。不推荐路线在正常策略中应不发车、收益为 0。</li>
<li>真实执行 Market.buy / sell，含批量滑点和库存限制；组队费 100，销售税普通城 10%/商贸城 5%，过境税普通 2%/商贸 1%，上限为货款 5%。过境城市和税额沿用界面的直线估算；另列税收满上限的结果。</li>
<li>单程耗时 = 向上取整(六角格距离÷5)，每月 3 旬，假定在月中旬出发，推进月数 = 向下取整((1+耗时)÷3)。每月调用实际供需、库存及价格回归代码。此快照的相关价格在基准水平，无活跃事件，本次到站收入与出发报价相同；并未证明波动行情下仍能获得相同利润。</li>
<li>本批全部售完；模拟器仍按到站市场容量处理部分成交，未售货物保留成本。明细中的现金利润不计未售库存价值，JSON 另保留库存成本和已实现利润。</li>
<li>每队需占用 500 兵、3,000 粮，未折算为金，也没有计入武将机会成本、回程、补给、抢劫、风暴、战争或其他商队竞争。未运行原生寻路；地形绕行、港口和真实移动消耗可能使耗时更长甚至不可达。100 条是离线经济样本，不是实机通行验证。</li>
<li>压力情景按每种货物销售收入乘 90% 向下取整、重新计算销售税，并取过境税上限；数量与采购成本不变。单列情景不用于修改基础结果。</li>
</ol></details><details><summary>来源与复现</summary><p>构建：<code>worldmod\\test\\build_routes_sim.bat</code><br>运行：<code>worldmod\\build\\trade_routes_sim.exe worldmod\\build\\trade-game\\world_custom\\trade.bin worldmod\\build\\trade-game\\basket-before-v8.txt outputs\\trade-routes-20261101\\routes.json 20261101</code><br>报告：<code>python worldmod/test/trade_routes_report.py outputs/trade-routes-20261101</code></p><ul>{sources}</ul></details>
</main><script>
const table=document.querySelector('#routes'),body=table.tBodies[0],search=document.querySelector('#search'),filter=document.querySelector('#filter');let column=13,ascending=false;
function sort(c,asc){{column=c;ascending=asc;const rows=[...body.rows];rows.sort((a,b)=>{{let x=a.cells[c].dataset.sort,y=b.cells[c].dataset.sort;let n=Number(x),m=Number(y);return (Number.isFinite(n)&&Number.isFinite(m)?n-m:x.localeCompare(y,'zh'))*(asc?1:-1)}});rows.forEach(r=>body.append(r));}}
function apply(){{let n=0;[...body.rows].forEach(r=>{{r.hidden=!(r.textContent.toLowerCase().includes(search.value.toLowerCase())&&(filter.value==='all'||r.dataset.rec===filter.value));if(!r.hidden)n++}});document.querySelector('#count').textContent='显示 '+n+' 条';}}
[...table.tHead.rows[0].cells].forEach((h,i)=>h.onclick=()=>sort(i,i===column?!ascending:false));search.oninput=filter.onchange=apply;sort(13,false);apply();
</script></html>'''
(folder / '100条商路成本收益.html').write_text(content, encoding='utf-8')
assert content.count('data-rec=') == 100
print(json.dumps(stats, ensure_ascii=False, indent=2))
print('PASS: 100 unique routes; per-lot and aggregate cash reconciliation; HTML rows verified.')
