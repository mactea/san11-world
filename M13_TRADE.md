# M13 贸易（第一期）：商队＝真实部队、行情、税与截击、电脑商队、存档

设计见计划（2026-10-24 定稿）：商队是地图上的真实运输部队，可被攻击、截击、护送；有将 / 无将（临时武将「商」）两种；
具体商品按史料定产地和基准价；枢纽城市行情恢复快、税低；电脑也做贸易。第一期用自动化命令就能玩通「买—运—卖」，
中地图贸易面板是第二期。

开关：`worldmod.ini` 的 `trade=1`（默认 0：**一个字节都不改**，与没有贸易代码的 DLL 完全一样，见「离线测试」）。

## 文件

| 文件 | 内容 |
|---|---|
| `data/trade_goods.json` | 商品、产区、价格曲线、市场组、枢纽城市（数据驱动，文件内有每种商品的史料出处） |
| `tools/trade_data.py` | → `trade.bin`（放在 data_dir，例如 `world_custom\trade.bin`）；`--report` 打印主要城市的价格表 |
| `worldmod/src/trade_model.cpp/.h` | 经济模型（不含游戏代码）：价格指数、买卖冲击、每月回落、电脑选路线 |
| `worldmod/src/trade.cpp/.h` | 游戏接入：商队创建 / 移动 / 抵达 / 过境 / 截击、电脑商队、钩子、TRAD 存档块、命令 |
| `worldmod/test/trade_sim.cpp` | 离线 120 个月模拟（用同一份 trade_model.cpp），`build_sim.bat` 编译 |

## 商品与价格（trade_goods.json → trade.bin）

14 种：絲、蜀錦、漆器、鐵器、馬、玉、胡椒、棉布、玻璃、珊瑚、乳香、皮毛、珍珠、鹽。每种有：

- `producers`：产地（按世界区域 `region`、光荣 42 城的州 `zhou`（`china_zhou` 表）或城市 id），等级 1..3（3 最便宜：×0.85，1：×1.2）；
- `base`：产地（等级 2）每「担」（load）的价格（金）；
- `exp` / `cap`：离最近产地每 100 格价格乘 (1 + d/100)^exp，封顶 cap 倍（地图上 1 格约 8–9 km，罗马—长安 1005 格）；
- `demand`：买方所在「市场组」（中國、草原、東夷、南海、天竺、安息、阿拉伯、羅馬、北方、沙漠）的需求倍数。

城市 c 的基准价 = min(产地价 × 距离倍数) × 需求倍数（产地本身不乘需求）。结果（`--report`）：絲 罗马 / 洛阳 28.5 倍，
胡椒 罗马 / 穆濟 18.5 倍；玻璃、珊瑚在长安是黎軒 / 西西里的 30 倍左右；鹽 cap 3，只适合中国境内短途。

**中国细分产区**：蜀錦（成都 3，江州 / 梓潼 / 永安 / 巴西 1）、涼州馬（武威 3，天水 / 安定 2）、冀州 / 南陽 宛 鐵、
青州 / 兗州 絲、海鹽（青州、南皮、會稽、吳；江州井鹽、樓蘭湖鹽）、交州 合浦 珍珠、益州 / 荊州 漆器。

**枢纽**（21 个）：洛陽、長安、敦煌、疏勒、大宛、藍氏（Bactra）、富樓、木鹿、斯賓（泰西封）、帕爾（巴尔米拉）、安都、
佩特、格拉、黎軒、羅馬、拜占、婆盧（Barygaza）、穆濟、扶南、交趾、番禺。枢纽买卖所有商品（转口接力），
市场大一倍，每月回落 `trade_revert_hub`（25%，一般城市 10%），过境税 / 关税 `trade_tax_hub`（5%，一般 10%）。

市场规模（担）：tier 1 / 2 / 3 = 300 / 180 / 100，光荣 42 城 250，枢纽 ×2。

### trade.bin（小端）
```
"TRDB" u32 version=1, u32 C, u32 G, u32 0
G × { char name[16] utf-8, u32 base, u32 0 }
C × { i16 lo, i16 hi, u8 flags (1 枢纽, 2 港口, 4 光荣中国), u8 市场组, u16 市场规模, char name[16] utf-8 }
C × G × u32 基准价（价格指数 1000 时每担的金）
C × G × u8  产地等级 0..3（0：不卖这种商品，枢纽除外）
```
C 必须等于 `cities=`（530），否则贸易不装（日志 ERROR）。

## 经济模型（trade_model.cpp）

- 价格指数 idx[c][g]，1000 = 基准价；买价 = 基准 × idx × (1 + spread/2)，卖价 × (1 − spread/2)（`trade_spread` 10%）。
- 买 q 担：idx × (1 + impact × q / 规模)，卖：idx ÷ 同样倍数，金额按买卖前后指数的平均价算（`trade_impact` 50%），
  指数限在 300..4000。
- 每月：idx 向 1000 回落差距的 10%（枢纽 25%）。
- 运量：陆路 `trade_cap_land` 20 担，两端都是港口 `trade_cap_sea` 60 担；每支商队 `trade_fee` 300 金。
- 有将商队：政治 ±5%（每点 0.1%）买卖价，智力 ±25% 运量；临时「商」按政治 60、智力 50。
- 钱从哪里来：卖货的收入是「市场」付的（游戏外的人口），所以贸易会造金；模拟里贸易净收入约为正常城市收入的 1.3%。

## 商队＝真实部队（trade.cpp）

### 创建（`createCaravan`）
1. 出发城必须是常规势力的据点，卖这种商品（产地或枢纽），金够（商品 + 手续费）。
2. 统帅：
   - 有将：该势力的武将，在出发城里（+0x9c == 城）；
   - 无将：照光荣贼兵 / 异族头目的做法（0x4af0e0）——`0x4aa610(模板, -1, -1)` 把模板武将（`trade_template`，默认 745 = 光荣的贼头目）
     复制进第一个空的特殊武将槽（PS+150..PT−1），+0x98 = 出发城，`0x4a32f0` 军团 = 出发城的军团，`0x4a5af0` 身份 3，
     姓改成「商」（Big5 B0 D3）、名 / 字 / 读音清空 → 地图上显示 **商隊**；能力 40/30/50/60/50，生卒 / 登场年照 0x4af0e0 设，
     再调 vfunc+0x34 重算。数量限制：同时 ≤ `trade_temp_max`（30），且创建后空闲特殊武将槽 ≥ `trade_temp_keep`（30）
     （光荣自己在空槽 > 30 时才生成贼 / 暴动，0x58e858）。
3. 部队：0x80 字节的出阵记录交给 `0x5883f0`（命令队列 0x96a59d8；逻辑线程调用时投递给主线程并等待，主线程调用时直接
   0x5db430——玩家出阵和事件脚本 create_unit 0x6772c0 用的是同一个记录）：+0 据点，+4 = 1（运输部队），+8 统帅，
   +0xc/+0x10 = −1，+0x14 金 0，+0x18 粮 `trade_food`，+0x1c 兵 `trade_escort`，12 个兵器槽 −1（运输部队陆上剣、水上走舸是写死的，
   0x495480 / 0x496160）。0x5db430 不从据点扣兵 / 金 / 粮；货款由贸易代码扣（0x4ae2a0）。
4. 命令：任务 1（移動）到目的城旁边 1..2 格内离部队最近的空格（无建筑 / 部队，地形不是崖 / 城 / 港 / 関）——
   `0x5a8d20(部队, 1, 坐标, −1)`（AI 命令执行器 0x96a9350），然后 `0x5ad980` 立刻走第一步；走不出城（返回 0）就
   `0x4bccd0` 撤掉（光荣对异族头目也是这样，0x4af3c7），不扣钱。
5. 扣金（0x4ae2a0 负数）、买入（行情上涨）、登记。

### 每旬（回合末钩子，逻辑线程）
- **抵达**：部队离目的城中心 ≤ `trade_arrive`（3）格 → 卖出（行情下跌），目的城不是本势力 / 同盟时收关税（同过境税率）给目的城，
  减去一路的过境税，余款加到出发城（出发城丢了就给本势力离它最近的城）；`0x4bccd0(部队, 0)` 撤掉部队：武将回本城（游戏自己的
  0x4a7990 + 任务 0x25），临时「商」由 0x4bccd0 自己回收（0x489d10 / 0x4acae0）。
- **过境**：部队进入另一座城的范围（最近的城，30 格内）时，按两势力关系：同盟免税；友好度 ≥ `trade_rel_tax`（40）交
  `trade_tax`%（枢纽 `trade_tax_hub`%）货值给该城；低于它有 `trade_rob`% 的机会被抢 `trade_rob_loss`% 的货，
  该城得到被抢货值的 `trade_loot`%。友好度 0x4814e0、同盟位 0x4811b0（R>42 时是 forces.cpp 的替换函数）。
- **消失**：部队没了 / 统帅换了 → 商队作废（日志「lost」或「intercepted」）；临时统帅若仍占着槽（名「商」、本军团、
  不在部队里、没有别的商队在用）就回收。
- **超时**：`trade_max_turns`（90 旬）还没到 → 解散，货丢失。
- 每旬重新下命令（目标格被占、城市形状变了都会换格）。玩家的商队：用游戏的「部隊任務一括実行」或手动走；
  `trade_player_auto=1` 时回合末自动走一步（未在游戏里测过，默认关）。
- **每月**：行情回落；电脑派商队。

### 截击（0x4bfa30 钩子）
部队被击破（0x4bfa30 → 0x4be790「壊滅」）时，若是商队：货值（按最近城市卖价）× `trade_loot`% 给胜方——攻击方是部队时
给它的本城（0x495520 统帅 +0x98），是据点时给该据点。**临时统帅永不被俘**：不走 0x4be790（那里会算俘虏），直接
`0x4bccd0(部队, 0)` 撤掉部队，特殊武将槽随之回收；有将商队照常走光荣的壊滅流程（武将可能被俘）。

### 电脑商队（每月）
从上次的位置起轮流看常规电脑势力（玩家的跳过），每月最多 `trade_ai_month`（6）支新商队，每个势力同时最多
`trade_ai_max`（2）支，只用临时「商」。势力里金 ≥ `trade_ai_min_gold` 的城，用 min(`trade_ai_invest`, 金 − `trade_ai_reserve`)
在 `trade_ai_range`（80）格内找「利润 / (距离 + 20)」最高的商品和目的地（利润 ≥ 成本的 `trade_ai_margin`%，扣掉预计关税），
不去会被抢的势力（关系「敌对」）的城。

**AI 部队规划钩子**（0x5def90，`trade_ai_hook=1`）：电脑每回合对每支部队调 0x5def90 定任务（之后 0x5ddcf0 → 0x5ad980 执行）；
是商队时只下贸易命令并返回 1，不让 AI 改成攻击 / 撤退 / 进城。关掉（0）时电脑会把商队当普通运输部队用。

## 钩子（全部只在 trade=1 时装；`verify_only=1` 只检查字节）

| 地址 | 原代码 | 做法 |
|---|---|---|
| 0x58051e | `call 0x4a1180`（游戏循环 0x580100：所有势力行动完，旬计数 world+0x5c += 1） | 改 call 目标：先调原函数，再做贸易的旬 / 月处理（逻辑线程） |
| 0x5def90 | `sub esp,20h; push ebx; push ebp`（AI 部队规划，thiscall(AI 势力对象, 部队) ret 4） | jmp：商队 → 下命令，返回 1；否则蹦床回原函数 |
| 0x4bfa30 | `mov eax,[esp+10h]; mov edx,[esp+0Ch]`（部队壊滅 0x4be790 的包装，(部队, 攻击方, a3, a4) ret 10h） | jmp：商队 → 缴获；临时统帅 → 0x4bccd0 代替原流程 |
| 0x57ff80 | `push esi; mov esi,ecx; mov eax,[esi+4]`（主循环每帧处理投递命令，thiscall(0x96a59d8, 1 参数) ret 4） | jmp：游戏等待玩家输入（0x57dcd0）时执行排队的自动化命令（send / ai），再回原函数 |
| 0x493BCE | （worldmod 已有的 loadedThunk） | translateLoaded 第一行调 `tradeStream`：TRAD 块 |

worldmod.cpp 只加了 5 行：`#include "trade.h"`、translateLoaded 里的 `tradeStream(stream)`、`TradeSetup` 和两处 `tradeInstall`。
automation.cpp 加了 `trade` 命令分发。

用到的游戏函数（ret 大小都核对过）：0x490e70 / 0x490b00 / 0x490d00 / 0x490aa0 取部队 / 武将 / 据点 / 势力，0x47a630 有效，
0x4917c0 / 0x491310 指针→编号，0x4ae2a0 加金（夹在 0..上限），0x486c80 金，0x4aa610 / 0x4acae0 临时武将，0x4a32f0 军团，
0x4a5af0 身份，0x4910e0 空闲特殊武将数，0x5883f0 出阵，0x4bccd0 撤部队，0x5a8d20 下任务，0x5ad980 执行任务，0x57dcd0 等待玩家，
0x48a7d0 / 0x489030 能力，0x489330 / 0x4898d0 / 0x489310 生 / 卒 / 登场年，0x4814e0 友好度，0x4811b0 同盟，0x495520 部队本城，
0x46ff20 / 0x470180 流读写，0x43a8f0 存档类流（6, 0xc, 0x11, 0x1a, 0x1b），0x43a8a0 剧本类流（2, 3, 0x16, 0x17）。
日期：world +8 年、+0xc 月、+0x10 日为开局日期，+0x5c 为经过的旬；天数 = (((年−1)×12 + 月−1)×3 + 旬)×10 + 日 − 1（同 0x4824b0）。

## 存档：TRAD 块

世界序列化 0x4937b0 在所有对象和地图之后到 0x493BCE（此时流的混淆 +0x68 已是 0）。存档类流（0x43a8f0）**写**时追加：
```
"TRAD" u32 version=1 u32 len
payload: u32 C, u32 G, i32 idx[C×G], i32 上次处理的月, u32 随机数状态, u32 电脑轮转位置,
         u32 n, n × { i32 unit, leader, force, corps, origin, dest, good, qty, cost, taxDue, born(天), lastCity; u32 flags(1 临时,2 玩家,4 被截) }
```
**读**：读流是整块缓冲（+0x14 当前，+0x18 末尾，0x46ff20 越界只置错误标志 +0x24）。先只看缓冲：剩余 ≥ 12 字节、以 "TRAD" 开头、
长度装得下才用游戏的读函数正好读掉这么多；否则（旧存档、别的尾部字节、截断）**一个字节都不读**，错误标志不动，贸易状态清空。
版本不认识：跳过块、状态清空。C / G 和 trade.bin 不同：行情重置、商队保留。读剧本（新游戏）清空状态；主数据流不理。
光荣读存档不检查流末尾（0x43ad00 读时只关闭），所以 trade=0 或旧 DLL 读带 TRAD 块的存档照常（块被忽略）。

## 自动化命令（worldmod_cmd.txt / tools/gamectl.py，需要 automation=1 或 auto_skip=1）

| 命令 | 说明 |
|---|---|
| `trade dump [文件]` | 写状态（商队、530 城 × 14 商品的买价 / 指数），默认 `worldmod_trade_dump.txt` |
| `trade send <出发城> <目的城> <商品> <担数> [武将编号]` | 城市 / 商品用编号或名字（如 `成都 洛陽 蜀錦 20`）；不写武将 = 临时「商」。排队到主线程，游戏等待玩家输入时执行（最多等 60 秒） |
| `trade ai` | 立刻跑一次电脑派商队（同样排队到主线程） |
| `trade tick [月数]` | 只让行情回落 n 个月（不动部队） |
| `trade target <城> <lo> <hi>` | 调试：位于 lo,hi 的商队会去哪一格 |

## ini（[san11pk_world]，worldmod.ini.example 里有说明）

`trade`（0）、`trade_data`（trade.bin）、`trade_temp_max`（30）、`trade_temp_keep`（30）、`trade_template`（745）、`trade_ai`（1）、
`trade_ai_hook`（1）、`trade_ai_month`（6）、`trade_ai_max`（2）、`trade_ai_min_gold`（6000）、`trade_ai_invest`（4000）、
`trade_ai_reserve`（2000）、`trade_ai_range`（80）、`trade_ai_margin`（25）、`trade_impact`（50）、`trade_spread`（10）、
`trade_revert`（10）、`trade_revert_hub`（25）、`trade_cap_land`（20）、`trade_cap_sea`（60）、`trade_fee`（300）、`trade_escort`（500）、
`trade_food`（3000）、`trade_arrive`（3）、`trade_max_turns`（90）、`trade_tax`（10）、`trade_tax_hub`（5）、`trade_rel_tax`（40）、
`trade_rob`（15）、`trade_rob_loss`（50）、`trade_loot`（60）、`trade_player_auto`（0）。值超范围或 trade.bin 缺失 / 不匹配：
日志 `trade: ERROR`，什么都不装。

## 离线测试

**经济模型**（`worldmod/test/trade_sim.exe trade.bin [月] [种子] [临时商队上限]`，同一份 trade_model.cpp）：530 城、
200 个势力（Voronoi）、随机关系、平坦收入，120 个月 + 36 个月不贸易：

```
trips 640 (lost 26), trade net 725822 gold = 1.26% of the normal income 57460800
return per trip: p10 27%  median 40%  p90 66%
price index range seen 648..1318; mean |idx-1000|: m12 0.41  m60 0.42  m120 0.47  after 36 months without trade 0.01
```
指数始终在界内、不漂移、停止贸易后回到基准；贸易钱 < 正常收入 10%；电脑每趟回报中位数 40%。240 个月、临时商队上限 100
的结果相同量级（1.1–1.3%）。

**harness**（真 DLL + 光荣 exe 映像在进程内，scratchpad `trade/harness`，`run_trade.py`、`compare_images.py`）：
- 所有补丁表 0 rejected（cities=530 forces=249 persons=14163 move=1）；
- 映像对比（忽略每次运行不同的 DLL / 堆地址）：无贸易代码的 DLL vs trade=0：**0 处差别**；trade=0 = 不写 trade 键 = trade=1 但
  trade.bin 缺失 = trade_temp_max 超范围；trade=1 比 trade=0 多且只多 4 处：0x58051f（call 目标）、0x5def90、0x4bfa30、0x57ff80；
  `trade_ai_hook=0` 时少 0x5def90；verify_only=1 trade=1：`hook sites ok`；
- TRADETEST（harness 链接同一份 trade.cpp，调游戏自己的流读写函数）：4 个钩子位置改写正确；回合钩子真的调了 0x4a1180
  （+0x5c 加 1）；0x57ff80 钩子栈平衡；TRAD 块读入后原样写回（逐字节相同）；无块 / 其它尾部字节 / 截断：不读、无错误标志、
  状态清空；版本 2：跳过；42 城的块：行情重置商队保留；主数据流不理；剧本流清空且不写块；5 种存档类流都写块；
  hexDist 与游戏的邻格函数 0x483a50 一致（2000 格 × 6 方向，1 步 / 2 步）；目标格选择（全草地、只剩一格、被占）；`trade dump`。

**离线做不到的**：harness 里没有加载剧本的世界（据点、武将、部队对象都没构造），所以商队的创建、移动、抵达、截击、
AI 规划钩子的实际效果只能在游戏里测。

## 游戏内要测的（先问用户）

1. `trade=1` 开局 / 读旧存档：日志 `trade: installed`，读旧档 `save without a TRAD block`。
2. `trade send 成都 洛陽 蜀錦 20`：出现「商隊」运输部队（名字、头像、所属势力 / 军团对不对），成都金减少，部队走出城；
   有将版 `trade send ... <武将编号>`。
3. 「部隊任務一括実行」让商队走；几回合后抵达：日志 `arrived`，出发城金增加，部队消失，武将回城、特殊武将槽回收
   （`trade dump` 里 free special officers 恢复）。
4. 过境：经过别的势力城附近时交税（日志）、敌对时被抢。
5. 截击：用部队打掉商队：胜方本城加金；临时「商」不进俘虏名单；有将商队武将可能被俘。
6. 电脑商队：每月日志 `AI sent n caravans`，电脑商队按命令走、不被 AI 改去攻城 / 进城；`trade_ai_hook=0` 对比。
7. 存档 → 读档：商队、行情还在（日志 `TRAD block v1 loaded`）；用 trade=0 读这个存档也正常。
8. 长时间运行（几十回合 / 几年）：特殊武将槽不泄漏、贼 / 异族照常出现、回合时间没明显变慢、没有崩溃。
9. `trade_player_auto=1`（回合末自动走玩家商队）是否稳定。

## 风险 / 未定

- 0x5883f0 / 0x5ad980 在回合末钩子里（逻辑线程，所有势力行动之后）调用：和光荣自己在回合处理里生成异族（0x4af0e0）是同一个线程、
  同一组函数，但调用时间点不同（光荣在它自己的回合处理 0x58ead7 / 0x59ad77 里）。
- 被击破以外的抓人方式（一騎討ち、特技捕縛）没有拦：临时统帅若被俘，下一旬发现部队没了时会回收它的槽。
- 贸易造金（市场付钱）；模拟里很小，但玩家大量有将商队（不受 30 支限制）可以更多。
- 临时「商」在商队存在期间算该势力的武将（武将列表里能看到）。
- 关系「同盟」用的是势力 +0x50 位集（成对设置 0x4b4f40）；「敌对」= 友好度低于 `trade_rel_tax`。
- 第二期：中地图贸易面板；第三期：季风、信息滞后。

## 部署（由用户决定何时做）

1. `python tools/trade_data.py --out "C:\Games\san11\Romance Of Three Kingdom 11 PK\world_custom\trade.bin"`
   （或复制 `data/world/trade.bin`）。
2. 复制 `worldmod\build\d3d9.dll` 到游戏目录（worldmod_*.txt 补丁表不变）。
3. `worldmod.ini` 的 `[san11pk_world]` 加 `trade=1`（其它键用默认即可；测试命令需要 `automation=1` 或现有的 `auto_skip=1`）。
