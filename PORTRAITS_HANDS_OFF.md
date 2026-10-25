# 三国志11 万人头像 · 端到端交接手册（Hands-Off）

> 用途：换一台机器、换一个会话，只看这一份就能继续「给名单里每个还缺头像的人物做一套三国志11风格头像，并装进游戏」这条流水线。
> 本文取代旧的 `PORTRAITS_HANDOFF.md`（旧文保留，只作为格式/容量逆向的历史参考）。
> 写于 2026-10-25，批次 0026 已安装完成、存档已补。**可变数字以 `data/persons/portraits/` 下的 JSON 为准，不以本文为准。**

---

## 0. 三分钟接手

```powershell
cd <人物工作树>                     # 例：C:\Users\<你>\...\san11world-persons
python tools/roster/verify_installed_portraits.py     # 应为 catalogued=N、无断言失败
```

然后读三个文件确认状态，再决定要不要继续：

| 文件 | 看什么 |
|---|---|
| `data/persons/portraits/work_state.json` | `completed_sets`、`next`、上次安装/备份路径、已知问题 |
| `data/persons/portraits/catalog.json` | 已安装条目数、`max(face)` → 下一个空闲 face |
| `data/persons/portraits/queue.json` | `status_counts.pending`、`source_sha256` 是否等于当前 `roster.json` 的 sha |

判断「还能不能接着跑」的三条硬条件：

1. `verify_installed_portraits.py` 全绿（说明当前游戏档案与剧本自洽）。
2. 没有残留的 `codex exec` 进程、没有正在跑的 `run_all.sh`。
3. 世界构建进程与游戏都没在运行（安装前必须满足，见 §9-I）。

---

## 1. 目标与当前状态

**目标（用户原话）**：把这批一万多人名单里没有头像的每个武将都做一套头像，风格参照《三国志11》（暗底、暖侧光、胸像、厚涂油画），人和人之间要有差异化，并应用到游戏中的对应武将；每做完一批照旧做导出、安装、回读校验，并把新头像写进存档。

**已完成**

| 批次 | 人数 | face 区间 | 状态 |
|---|---:|---|---|
| 早期 0001–0022、初始 4 人（face 2196–2199） | 1212 | 2196–2608 | 已安装 |
| 0023（100 人） | 100 | 2509–2608 | 已安装 |
| 0024 | 1001 | 2609–3608 | 已安装 |
| 0025 | 2000 | 3609–5608 | 已安装、全量回读通过、已写存档 |
| 0026 | 1000 | 5609–6608 | 已安装、全量回读通过、已写存档（2026-10-25 08:53） |

* 已安装总量：**4212**。下一个空闲 face：**6609**。
* 游戏侧扩展档案：`San11Face10.fce`（2400–3399）、`11`（3400–4399）、`12`（4400–5399）、`13`（5400–6399，已满）、`14`（6400–7399，已用 6400–6608）。
* 队列（2026-10-25 08:52 刷新，对应 13313 人名单）：`installed 4212 / pending 9101`。
* **未验证**：游戏内 Direct3D 实际渲染。文件级校验 ≠ 画面验收，交接时不能把前者说成后者。

---

## 2. 新机器需要准备什么

| 依赖 | 要求 | 说明 |
|---|---|---|
| 操作系统 | Windows 10/11 | 导出脚本用 System.Drawing，工具链含 `.bat`/`.ps1` |
| Python | 3.11+ | `pip install -r requirements.txt`（Pillow 必需；`imagecodecs` 只在读 ETOPO 高程时需要） |
| PowerShell | Windows PowerShell 5.1 或 pwsh 7+ | `tools/roster/export_portrait.ps1` 用 `System.Drawing` |
| Codex CLI | 0.163.0（或更新）且已登录 | 用**内置 `image_gen`**，不需要 DashScope/API key；`codex exec` 默认路径见 §9-H |
| 游戏 | 三国志11 PK + 本 mod | 需要 `san11pk_world.exe`、`d3d9.dll`（worldmod 版）、`worldmod.ini`、`world_custom/` |
| 磁盘 | ≥ 30 GB 空闲 | 每 1000 人：`build/<batch>/raw` ≈ 2.1 GB，`data/persons/portraits/rotkNN_*` ≈ 2.6 GB |

验证 Codex 可用（会真的生成一张图，约 1–3 分钟）：

```powershell
codex exec --skip-git-repo-check -s read-only -C . "Use your built-in image_gen tool exactly once. Prompt: a single plain red circle on white. Reply with only the generated exec-*.png path."
```

图片落在 `C:\Users\<你>\.codex\generated_images\<session>\exec-<uuid>.png`，尺寸 1254×1254。

---

## 3. 目录地图

| 用途 | 位置 |
|---|---|
| 人物工作树（本文所在） | `C:/Users/garib/Documents/Work/CMCode/san11world-persons` |
| 主项目树（世界/地形/城市，不是人物线） | `C:/Users/garib/Documents/Work/CMCode/san11world` |
| 游戏目录 | 见 `project.json` 的 `game_dir`（本机 `C:/Games/san11/Romance Of Three Kingdom 11 PK`） |
| 权威名单 | `data/persons/roster.json`（JSON 数组，13313 条） |
| 头像根目录 | `data/persons/portraits/` |
| 安装目录表 | `portraits/catalog.json`（每人一行：name/birth/face/folder/batch/…） |
| 全队列 | `portraits/queue.json`（roster 快照 + 每人 status） |
| 进度说明 | `portraits/work_state.json`（人读；接手第一站） |
| 最近一次全量校验 | `portraits/verification.json` |
| 安装报告 | `portraits/batches/<manifest名>_installed.json` |
| 批次计划/提示词 | `build/rotk11_batch_00NN/{selection.json,plan.json,manifest.json,research.json,raw/}` |
| 生成驱动（复用） | `build/codex_batch_0023/run_codex_batch.py` |
| 安装/校验/存档工具 | `tools/roster/{install_portrait_batch,verify_installed_portraits,patch_save_faces,portrait_queue}.py` |
| 导出工具 | `tools/roster/export_portrait.ps1` |
| 游戏备份 | `<game_dir>/portrait_backups/…` |

每个头像文件夹（`portraits/rotk26_001_5609/`）内容固定：

```
portrait_master.png      1254×1254 原图（≈2.27 MB，最大）
portrait_240x240.png/.bmp  240×240 24 位
portrait_64x80.png/.bmp     64×80
portrait_32x40.png/.bmp     32×40   （仅交付用；游戏档案里的两张小图都是 64×80）
```

---

## 4. 名单数据库（roster）

* `data/persons/roster.json`：一个 JSON 数组，一人一条。关键字段：

  | 字段 | 含义 |
  |---|---|
  | `id` / `qid` | 内部 id / 维基 Q 号。**会随名单修订变化，禁止当身份键** |
  | `name_full` / `name_orig` | 全名（Big5 可编码）/ 原名变体 |
  | `sei` / `mei` | 姓 / 名（写进剧本的名字就是 `sei+mei`） |
  | `birth` / `death` / `appear` | 生年 / 卒年 / 登场年（游戏内年份） |
  | `role` / `region` / `polity` / `sex` | 身份（武将/文臣/后妃/外族首领/叛军首领/方士/谋士/…） |
  | `offices` / `stats` / `skill` / `apt` / `bio` | 官位、五维、特技、适性、列传 |
  | `face` | 头像编号；`-1` 表示未分配。安装器会回写 |
  | `src` / `keys` / `conf` | 来源、来源键、可信度 |

* **身份唯一键 = `(name_full, birth)`**。所有工具（选人、安装、校验、存档补丁）都按它匹配；`id` 变化不影响。
* 名单由另一条线（M10）持续维护，人数、生卒年、id 都可能变。详见 `M10_ROSTER_HANDOFF.md` 与 `M10_PERSONS.md`；**不要为了头像去改名单内容**，安装器只会回写 `face` 字段。
* 游戏侧：`worldmod.ini` 的 `[san11pk_world] persons=14163`、`faces=16000`；部署剧本 15 个，位于 `world_custom/scenario/`，人物记录 152 字节（name +0、birth +58、face +53）。
* 已知数据问题（不要私下「修好」，报给用户）：
  * `郄儉` / `郤儉`（同为 birth 182）在名单里是两条记录，剧本里能命中两条 → 安装器会断言失败。选人阶段自动跳过。
  * 有 29 人不在任何部署剧本里、11 人在名单里有 2 条同 (name,birth)、9 条队列重复、2 人剧本内命中 2 次。`select.py` 会统计这些跳过原因，属正常。

---

## 5. 一批头像的标准流程（九步）

所有路径从人物工作树根目录起算。批次号、起始 face、人数是唯一需要改的三个量。

### Step 0 · 刷新队列（每次选人前必做）

```powershell
python tools/roster/portrait_queue.py
```

`queue.json` 是 roster 的快照。roster 增长后不刷新，新人永远进不了队列。刷新后核对：

```powershell
python -c "import json,hashlib,pathlib;b=pathlib.Path('data/persons/roster.json').read_bytes();q=json.load(open('data/persons/portraits/queue.json',encoding='utf-8'));print(len(json.loads(b)),q['total'],q['status_counts'],hashlib.sha256(b).hexdigest()[:12]==q['source_sha256'][:12])"
```

要求：两个人数相等，sha 前 12 位一致，`status_counts.pending` 是本次可用的池子。

### Step 1 · 选人（`build/rotk11_batch_00NN/select.py`）

从 `build/rotk11_batch_0026/` 复制整个目录为下一批，改这几处：

| 位置 | 改成 |
|---|---|
| 文件头 docstring | 批次号、人数、起始 face |
| `COUNT` | 本批人数（当前节奏 1000） |
| `FIRST_FACE` | 当前 `catalog.json` 的 `max(face)+1` |
| `folder=f"rotk26_{...}"` | `rotk27_…` |
| `batch='0026'` | `'0027'` |

选人规则（`select.py` 已实现，不要放宽）：按队列顺序取 `pending`；跳过重复 `(name,birth)`；要求 roster 恰好 1 条同键记录；要求剧本里恰好 1 条同键记录；face 从 `FIRST_FACE` 连续分配并断言未被占用。

```powershell
cd build/rotk11_batch_0027
python select.py        # 输出 selected / faces / skipped 统计
```

通过标准：`selected == COUNT`，否则说明池子里可装的人不够，需要向用户报告而不是改断言。

### Step 2 · 预检（`precheck.py`）

```powershell
python precheck.py
```

逐人、逐剧本核对 `(sei+mei, birth)` 唯一命中。要求输出 `people_with_problems: 0`（同时写 `precheck.json`，应为 `{}`）。
安装器对 15 个剧本每个人都做同样断言；预检失败的人不要硬装。

### Step 3 · 写提示词（`make_plan.py`）

`build/rotk11_batch_0026/make_plan.py` 是当前版本，复制后只改 `batch`、key 前缀（`rotk27_`）、输出文件名。风格块与差异化层**不要动**，否则风格会飘（见 §6）。

```powershell
python make_plan.py    # 生成 plan.json / manifest.json / research.json
```

自检：

```powershell
python -c "import json;p=json.load(open('plan.json',encoding='utf-8'));print(len(p['people']), len({x['prompt'] for x in p['people']}))"
```

两个数字必须相等（提示词两两不同）。

### Step 4 · 生成（12 并发）

```powershell
sh run_all.sh > driver.log 2>&1          # Git Bash；在 Claude Code 里用后台任务跑
```

`run_all.sh` 做的事：反复调用

```
python ../../build/codex_batch_0023/run_codex_batch.py --workers 12 --plan plan.json --dir .
```

直到 `raw/*.png` 达到目标数；每轮跳过已完成的图（判据：文件存在且 >100 KB），所以中断/重启零成本；连续 3 轮无新增则停。单张失败自动重试 2 次（间隔 10s/30s）。

* 单图耗时：中位数 ~110–170 s；12 并发实测 **≈4.4 张/分**（1000 张 ≈3.8 h），0 错误。
* 进度看 `ls raw/*.png | wc -l` 与 `codex_log.jsonl` 的 `"status": "error"` 计数，不要只看驱动日志最后一行。
* 监工建议：按「每 50 张」报一次，否则 30 分钟窗口里几乎看不到事件；判断掉速要用 10 分钟桶（文件 mtime），不要用两次读数相减。

### Step 5 · 导出三尺寸

先生成绝对路径清单（`export_all.ps1` 有路径拼接 bug，**不要用**，见 §9-A）：

```powershell
python -c "
import json,pathlib
root=pathlib.Path(r'<人物工作树绝对路径>')
m=json.load(open('build/rotk11_batch_0027/manifest.json',encoding='utf-8'))
rows=[]
for p in m['people']:
    rows.append(str(root/'data/persons/portraits'/p['folder'])+'\t'+str(root/'build/rotk11_batch_0027/raw'/f\"{p['face']}_{p['name']}.png\"))
open('build/rotk11_batch_0027/export_pairs_abs.tsv','w',encoding='utf-8').write('\n'.join(rows)+'\n')
print(len(rows))
"
```

再逐行导出：

```powershell
$pairs = 'build\rotk11_batch_0027\export_pairs_abs.tsv'
$script = '.\tools\roster\export_portrait.ps1'
foreach ($line in Get-Content -LiteralPath $pairs -Encoding utf8) {
    $parts = $line.Split("`t")
    if (-not (Test-Path -LiteralPath $parts[1])) { Write-Host "MISSING $($parts[1])"; continue }
    & $script -Folder $parts[0] -MasterImage $parts[1]
}
```

通过标准（每人 7 个文件）：

```powershell
python -c "
import json,pathlib
root=pathlib.Path('.')
m=json.load(open('build/rotk11_batch_0027/manifest.json',encoding='utf-8'))
need=['portrait_master.png','portrait_240x240.png','portrait_240x240.bmp','portrait_64x80.png','portrait_64x80.bmp','portrait_32x40.png','portrait_32x40.bmp']
bad=[p['folder'] for p in m['people'] if not all((root/'data/persons/portraits'/p['folder']/n).exists() for n in need)]
print({'people':len(m['people']),'bad':len(bad),'sample':bad[:5]})
"
```

### Step 6 · 安装（停机窗口）

**前置：世界构建进程与游戏都必须关闭**（否则并发守卫会中断，见 §9-B）。

```powershell
Get-Process san11pk_world, san11pk -ErrorAction SilentlyContinue   # 必须为空
$env:PYTHONIOENCODING='utf-8'
python tools/roster/install_portrait_batch.py build/rotk11_batch_0027/manifest.json
```

安装器会：按 `(name,birth)` 校验名单唯一；校验 face 范围与占用；为每个剧本定位唯一记录并只改 face 两个字节；按 `(face-2400)//1000` 写入/新建 `San11Face1N.fce`（编码 240×240 + 两张 64×80 的 WFTX 载荷）；**写盘前**断言名单与 15 个剧本、所有 bank 都没被别人改过；写盘后逐条回读载荷；最后回写 `roster.face`、追加 `catalog.json`、写 `batches/<manifest名>_installed.json`、创建 `portrait_backups/batch-<时间戳>/rollback.json`。

通过标准：进程退出码 0，且报告 JSON 里 `people` 数量正确、`backup` 路径存在。（退出码 0 不代表没抛异常——见 §9-E，要读输出。）

### Step 7 · 全量回读校验

```powershell
python tools/roster/portrait_queue.py          # 把本批标成 installed
python tools/roster/verify_installed_portraits.py
```

要求输出形如：

```json
{"catalogued": 4212, "variants_verified": 12636, "scenario_bindings_verified": 63180, "runtime_rendering_verified": false}
```

`catalogued` = 已安装总数，`variants_verified` = 3×，`scenario_bindings_verified` = 15×。任何断言失败都要先查「名单是否刚被改过」，不要改断言。

### Step 8 · 写进存档

剧本只决定新开局；已有存档里有自己的人物副本，必须单独补。

```powershell
$sd = '<game_dir>\UserData_kit\Koei\San11 Tc\SaveData'
python tools/roster/patch_save_faces.py --dry-run "$sd\Save000.s11" "$sd\Save001.s11"
python tools/roster/patch_save_faces.py "$sd\Save000.s11" "$sd\Save001.s11"
```

判据：`matched` = 剧本人物数；`changed` = 本批新增（含历史漏补）；每份存档写前自动备份到 `portrait_backups/save-<时间戳>/`。脚本按 `SCEN001.S11` 的人物顺序定位存档里的数组（`base=0x488a4`，stride 203），并对解包/回包做逐字节断言，只允许 face 两字节变化。

### Step 9 · 记账

更新 `data/persons/portraits/work_state.json`（人读，写给下一个会话）：

* `completed_sets` / `pending_sets_at_last_snapshot` 改成验数后的真实值；
* `next`：写清本批 face 区间、角色分布、女性数、新建的 bank、下一个空闲 face、剩余队列；
* 新增一条「本轮验证」事实（verify 输出的四个数 + 存档补丁的 changed/already）；
* 新的已知问题（若有）写进去，不要只留在对话里。

队列与目录表由 Step 6/7 的脚本自动更新，不要手改。

---

## 6. 提示词方案：风格 + 差异化

来源链：`build/qwen_batch_0023/make_plan.py`（身份、年龄、服饰）→ `build/rotk11_batch_0025/make_plan.py`（ROTK11 风格块）→ `build/rotk11_batch_0026/make_plan.py`（差异化层）。

四个固定块（不要改，改动会破坏与已装 3212 张的一致性）：

1. **构图**：1:1 胸像；顶部 9% 纯背景，冠帽最高点 7–10%，双眼约 34%，下巴 56–62%，双肩完整，左右各留 ≥10% 背景，头宽 45–55%。
2. **风格**：古典写实油画、厚涂、可信骨骼、克制大地色；背景只有深棕黑烟雾与暗角；右前方暖侧光，一侧入暗，肩颈/冠帽有暖色边缘光。
3. **画质**：眼睛清晰、面部干净、无文字/水印/边框/签名。
4. **负面词**：由生成器给出（现代、3D、动漫、幻想盔甲、明清服饰等）。

差异化层（全部由 `hash(name+birth)` 决定，确定性、可复现）：

| 维度 | 取值 |
|---|---|
| 肤色/气色 `SKIN` | 6 种（偏白偏冷 / 微黑日晒 / 偏黄清癯 / 红润 / 苍白唇淡 / 健康风霜细纹） |
| 武将头饰 `SOLDIER_HEADS` | 4 种（平巾帻+抹额 / 铁青圆顶小盔+红缨 / 深色软巾+暗铁抹额 / 黑色介帻+窄铜边） |
| 武将甲胄 `SOLDIER_ARMOURS` | 4 种（深褐铁札 / 黑漆铁札 / 青铜旧铁 / 暗红漆皮+圆护心） |
| 叛军（非黄巾）`RAIDER_ARMOURS` | 3 种 + 2 种头饰 |
| 外族首领 | `STEPPE_HATS` 3 种、`STEPPE_ROBES` 2 种（毛皮暖帽/羊皮软帽/黑毡帽；毛皮镶边长袍/灰蓝厚毡袍+狼毛领） |
| 文官袍色 `CIVIL_ROBES` | 8 种（深青灰/深赭褐/墨黑/暗橄榄/灰蓝/深灰紫/土黄褐/青黑），仅用于非常任高官的文臣 |
| 方士 / 谋士 | 固定道袍 / 纶巾儒袍（不参与随机） |

`seed = face`（同一 face 重跑同图）。改名或改 birth 会换一套差异化，这是有意的。

视觉抽检：每批生成 1/3 与 2/3 时各拼一张 8×8 联络表看构图与差异化，再抽 64×80 看小图可读性。联络表可用 Pillow 现拼，无需专门脚本。

---

## 7. FACE 档案格式与 bank 规则

扩展档案位置：`<game_dir>/UserData_kit/Koei/San11 Tc/FaceData/San11Face1N.fce`（**不要放进 `Media/face`**）。

* 文件头 20 字节：`FACE` + `<4I>`(64, firstFace, faceCount, 3×faceCount)。
* 之后 `faceCount` 条 4 字节 metadata（从原档案 `San11Face00.fce` 的模板人 2000/2100 复制）。
* 图像表从 `20+4*count` 起，每项 `<offset,size>` 两个 uint32。
* 图像索引：大图 = `local`；小图 = `count+2*local` 与 `count+2*local+1`（两张都是 64×80）。
* 载荷：`WFTX0010`，24 字节头（宽 u16@16、高 u16@18、24bpp），像素为**自底向上 BGR**。
* bank 映射：`number=(face-2400)//1000`，`first=2400+number*1000`，文件 `San11Face{10+number:02d}.fce`。
  → 10: 2400–3399，11: 3400–4399，12: 4400–5399，13: 5400–6399，14: 6400–7399。
* 容量：`worldmod.ini faces=16000`（bin 补丁已部署）。理论上限 16000，每 bank 1000。
* 早期试验里出现过的错误格式（64 字节头、offset/size 颠倒、小图 32×40）全部作废，不要复用。容量逆向细节见 `work_state.json` 的 `capacity_findings` 与旧 `PORTRAITS_HANDOFF.md`。

---

## 8. 存档结构

* 存档：`<game_dir>/UserData_kit/Koei/San11 Tc/SaveData/Save00N.s11`。
* 容器：`MWC` + u8 + u32 原始大小 + u32 压缩大小 + zlib 数据（标准 zlib）。
* 解包后的人物数组：stride **203**（剧本是 152），name +0（10 字节）、birth +58、face +53，基址 `0x488a4`，用剧本前四个人的 `(name,birth)` 定位。
* `patch_save_faces.py` 只改 face 两字节，并做「解包→回包→逐字节比对＝只动了 face」断言；写前备份。
* 存档会被玩家继续游玩而改变；补丁按 `(name,birth)` 匹配，不依赖旧偏移之外的假设。**游戏运行时不要写存档文件。**

---

## 9. 实测踩过的坑

**A. `build/qwen_batch_0023/export_all.ps1` 有 bug**：它对 TSV 首列做 `Join-Path $OutRoot $parts[0]`，传绝对路径会拼出 `…\portraits\C:\…`。**改用 §5 Step 5 的逐行 `-Folder/-MasterImage` 调用**。

**B. 安装器有并发守卫，而且是有意的**：写盘前断言名单、15 个剧本、所有 bank 的字节未变。第一次 0025 安装就因为在编码 6000 张图时世界构建进程重写了剧本而中止（好在中止发生在任何写入之前，banks 时间戳未变）。**install 前先确认 `san11pk_world` 与游戏没在跑，装完再让用户启动。**

**C. bank 编号别凭直觉**：判断依据是安装器的 `(face-2400)//1000`，不是「3400 起就该换」。写文档/汇报时用 §7 的对照表。

**D. 监视器会制造「掉速」错觉**：Monitor 30 分钟必过期，按 50 张一报时一个窗口只能报两次；两次读数相减会得出「30 分钟只涨 15 张」。**用文件 mtime 做 10 分钟桶**判断真实吞吐（正常 40–48/桶）。

**E. PowerShell 退出码 0 可能掩盖 Python traceback**：`& python …` 后再判断 `$LASTEXITCODE`，或直接读输出；不要只看「[exited with code 0]」。

**F. 路径含中文与空格**：一律 `-LiteralPath`，TSV 用 UTF-8（无 BOM），Python 里 `encoding='utf-8'`；打印到 Git Bash 控制台时中文显示乱码但文件内容正确。

**G. 队列会过期**：`queue.json` 的 `source_sha256` 与 roster 不一致时，新人物不在队列里，`pending` 数量偏小。每批选人前先跑 `portrait_queue.py`。

**H. 两处硬编码路径（换机器必须改）**：
* `build/codex_batch_0023/run_codex_batch.py` → `CODEX = r'C:\Users\garib\AppData\Local\Programs\OpenAI\Codex\bin\codex.exe'`（还有一处默认 `PLAN`，但 `--plan` 会覆盖）。
* `tools/roster/patch_save_faces.py` → 找剧本时写死了 `C:\Games\san11\…\world_custom\scenario`，应改成 `GAME / cfg[…] / 'scenario'`（与安装器一致）。

**I. 生成结果对象很大**：早期通道里生成返回可能带巨大 base64，**不要整对象打印**；只取 `exec-*.png` 路径或文件。

**J. 已完成的图不会重做**：判据「存在且 >100 KB」。要重做某张，先删 `raw/<face>_<name>.png`（或整个 raw 里那一张），再跑驱动。

---

## 10. 失败、恢复与回滚

| 现象 | 处理 |
|---|---|
| 单张生成失败 | 自动重试 2 次；仍失败则该轮记 `failed`，下一轮 `run_all.sh` 会补 |
| 整轮无进展 3 次 | `run_all.sh` 自行停止；先查 Codex 登录/额度/网络，再手工跑一次 `--workers 4` 定位 |
| 安装断言 `game identity` 失败 | 该人在某个剧本里命中 0 或 2 条。查名单是否刚改、是否变体写法（郄/郤）。**不要改断言绕过** |
| 安装断言「并发修改」失败 | 有进程在写剧本/名单。确认世界构建与游戏已停止后**重跑同一条命令**（写入是原子的前置检查，未产生半成品） |
| 安装中途 `Roster changed during file installation` | 说明编码/写盘期间 roster 被改。重跑；若反复出现，先在名单线上冻结写操作 |
| 校验失败 | 打印的是具体人物；先核对 `(name,birth)` 是否仍唯一，再决定是重装还是等名单稳定 |
| 存档补丁失败 | 用 `--dry-run` 复现；`base` 找不到说明存档结构变了（换版本/新存档），不要硬改 |

**回滚**：`portrait_backups/batch-<stamp>/rollback.json` 记录每个 bank 的原长度/哈希/逐字节改动，以及每个剧本的原哈希与旧值；`save-<stamp>/` 是存档原件。回滚时**只回退该批次的字节**，不要用整文件覆盖，否则会把后续批次一起抹掉。世界树里 `build/portrait_native` 有指向游戏目录的 junction，删除前确认绝对路径。

---

## 11. 验收标准

一批完成的证据（缺一不算完）：

1. `raw` 数量 = 计划人数，`codex_log.jsonl` 无 `error`（或有记录且已补齐）。
2. 每人 7 个导出文件齐全；抽查 64×80 可读。
3. `install_portrait_batch.py` 成功且报告人数正确、备份目录存在。
4. `verify_installed_portraits.py`：`variants_verified = 3×catalogued`、`scenario_bindings_verified = 15×catalogued`，无断言失败。
5. 两份存档 `matched` 正确、`changed + already = 已装人数`，备份存在。
6. `work_state.json` 与 `queue.json` 计数一致。

**仍然未验证的**：游戏内画面渲染（Direct3D）。历史上隐藏测试都在 `Direct3DCreate9/CreateDevice` 处退出，对照组（未扩容 DLL、faces=2400）同样复现，所以那不是扩容导致的，但也**没有**画面级证据。交接时必须如实说「文件级已验，画面未验」，需要用户启动游戏亲自确认。

---

## 12. 搬到另一台机器

### 12.1 从 git 拿到的

```bash
git clone <repo> san11world-persons
git checkout m10-persons
```

`.gitignore` 排除了 `build/`、`project.json`、`portrait_backups/` 等，`data/persons/portraits/` 目前**未入库**，所以下面两项必须手工搬。

### 12.2 必须手工复制的

| 内容 | 大小（参考） | 说明 |
|---|---|---|
| `data/persons/roster.json` | ~20 MB | 权威名单（含已回写的 face） |
| `data/persons/portraits/{catalog,queue,verification,progress,work_state,identity_review}.json` + `batches/` | ~12 MB | 状态与报告 |
| `data/persons/portraits/rotk2*/`（以及仍在用的 `rotk23_*`） | 每人 ≈2.6 MB | 已装头像的三尺寸文件 |
| `build/.*batch_00NN/raw/` 或 `portrait_master.png` | 每人 ≈2.1 MB | 两者是同一张图，**只带一份**；带 master 省空间，导出可重跑 |
| 游戏侧 `UserData_kit/Koei/San11 Tc/FaceData/San11Face1*.fce` | 数十 MB | 已装 bank；不带就要重跑安装（需原始 2400 档案 + 素材） |
| 游戏侧 `world_custom/scenario/*` | 视场景而定 | 已绑定的 15 个剧本；不带就要重装 |
| 游戏侧 `worldmod.ini`、`d3d9.dll`、`san11pk_world.exe` | — | mod 本体，见主项目 `worldmod/` 的构建说明 |
| `project.json` | 1 KB | 不入库，按 §2 新建：`{"game_dir": "<新机器游戏路径>", ...}` |

**最小传输**：只带 `portrait_master.png`（每张 2.27 MB）+ 游戏侧 bank/剧本，到新机器后按 Step 5 重跑导出，再按 Step 7 校验。

### 12.3 新机器落地检查

```powershell
# 1) 依赖
python -c "import PIL; print(PIL.__version__)"
codex --version

# 2) 配置
copy project.example.json project.json    # 改 game_dir
python -c "import json,pathlib;from tools.roster.install_portraits import GAME;print(GAME.exists())"  # 从仓库根跑
```

```powershell
# 3) 改两处硬编码（§9-H）
notepad build/codex_batch_0023/run_codex_batch.py     # CODEX = ...
notepad tools/roster/patch_save_faces.py              # 剧本路径改成 GAME/...

# 4) 状态自洽
python tools/roster/portrait_queue.py
python tools/roster/verify_installed_portraits.py
```

`verify` 全绿即可从 §5 的下一批继续；若不绿，先解决身份/剧本漂移，不要开始生成。

---

## 13. 下一批（0027）待办清单

1. `python tools/roster/portrait_queue.py`（0026 后已刷到 13313 人 / pending 9101；名单若再增长仍要先刷）。
2. 读 `catalog.json` 取 `max(face)`，`FIRST_FACE = max+1`（当前为 **6609**）。
3. `cp -r build/rotk11_batch_0026 build/rotk11_batch_0027`，按 §5 Step 1 的表改 6 处（COUNT/FIRST_FACE/前缀/batch/文件头/docstring）。
4. 依次跑 `select.py` → `precheck.py` → `make_plan.py`，检查 selected=1000、problems=0、提示词唯一。
5. `sh run_all.sh > driver.log 2>&1`（12 并发，预计 ≈3.8 h）。
6. 生成完按 Step 5→9 走：导出、安装（先停机）、校验、补两份存档、记账。
7. 向用户报告时给：本批 face 区间、角色分布、verify 四个数、存档 changed/already、备份路径、**明确说明画面未验**。

预计每批 1000 人 ≈3.8 h 生成 + ≈0.5 h 导出安装校验。队列池当前 **pending 9101**（13313 人名单），roster 再增长时刷新后另计。

---

## 14. 给新助手的一段话（可直接粘）

> 你在 `san11world-persons` 工作树，任务是继续给名单里缺头像的人物做三国志11风格头像并装进游戏。先读 `PORTRAITS_HANDS_OFF.md`，再读 `data/persons/portraits/work_state.json`、`catalog.json`、`queue.json`，并跑 `python tools/roster/verify_installed_portraits.py` 确认当前部署自洽。
> 规则：身份只用 `(name_full, birth)` 匹配，绝不用 id；生成只用 Codex CLI 内置 `image_gen`（同 `build/codex_batch_0023/run_codex_batch.py`），不要换 API/模型；提示词的构图/风格/画质块与差异化层不要改；安装只在世界构建和游戏都停止时做，安装器断言一条都不能跳；文件校验不等于画面验收。每批照 §5 九步走，并把真实计数写回 `work_state.json`。
> 并发 12，按 50 张一档监工，用 10 分钟文件桶判断吞吐。遇到数据问题（同键两条名单、剧本缺人、变体写法）报告给用户，不要私下改名单或放宽断言。
