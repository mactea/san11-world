# 三国志11万人名单头像任务交接（hands-off / handoff）

> **已被 `PORTRAITS_HANDS_OFF.md` 取代（2026-10-25）。** 接手请先读那份；本文保留，只作 FACE 格式、容量逆向与早期历史的参考。

> 用户原始目标：把这批一万多人的名单中没有头像的每个武将都做一套头像，并应用到游戏中的对应武将。用户于2026-10-23要求：做完当前轮次后暂停，并留下本交接文档。任务没有完成；暂停不代表缩小目标。
>
> 本文件是接手入口；可变计数以 `data/persons/portraits/progress.json`、已部署验证以 `verification.json` 为准。恢复前先读取这两个文件和 `work_state.json`，不能把本文中的历史计数当作实时状态。

## 1. 当前状态与边界

暂停快照（2026-10-23 11:34，批次 `0021` 完成）：名单12825人，已安装108条武将头像配置，待处理12717条。全量回读通过：324个WFTX载荷、15剧本合计1620处人物绑定。没有未结束的图像生成或安装进程。本轮5张新画像和1条异名配置；张宴/张晏共用同一美术原图。

| 本轮武将记录 | face | 资产文件夹 |
|---|---:|---|
| 張晏（148） | 2498 | `han_zhangyan_western_alias`（复用張宴原图） |
| 曹寬（148） | 2499 | `han_caokuan` |
| 谷永（148） | 2500 | `han_guyong_yulin` |
| 來豔（149） | 2501 | `han_laiyan` |
| 宗俱（149） | 2502 | `han_zongju` |
| 張舉（149） | 2503 | `han_zhangju_rebel` |

本轮计划、manifest和安装报告均在 `data/persons/portraits/batches/0021*`；备份为游戏 `portrait_backups/batch-20261023-113447`。下一批编号0022、下一空闲face2504；名单顺序的下一候选是李鹹(149)、王越(149)、竺朔佛(149)、董扶(149)、董萌(149)，恢复时再次核对身份与最新队列。王越的当前传记包含传说性内容，不能直接当成可靠史实。

- 已完成的资产有本地原图、三个尺寸的PNG与24位BMP，并写入FACE档案和15个剧本的人物头像字段。
- 文件载荷与15个剧本绑定可逐项回读验证。**游戏内真正显示头像仍未验证通过**，不能把文件校验称为游戏画面验收。
- 最新名单一直被其他工作更新，人数、人物ID、出生年都可能变化。不要覆盖并发修改，不要依赖旧ID。
- 游戏部署仍是旧 `persons=12613` 剧本，最新扩展名单已达12825；完整迁移新名单到全部剧本尚未做完。遇到新人物在旧剧本不存在，不能绕过安装器断言冒称已应用。
- 旧存档没有迁移；当前绑定修改作用于部署的剧本及其新开局。
- 不要继续运行任务，除非用户明确恢复。恢复后继续完整目标，不需重新请求已经授权的头像生成、备份及游戏写入权限。

## 2. 工作目录与关键文件

| 用途 | 位置 |
|---|---|
| 实际工作树 | `C:/Users/garib/Documents/Work/CMCode/san11world-persons` |
| 原会话目录（不是人物工作树） | `C:/Users/garib/Documents/Work/CMCode/san11world` |
| 游戏目录 | `C:/Games/san11/Romance Of Three Kingdom 11 PK` |
| 项目配置 | `project.json`，读取其中 `game_dir`，不要自行猜另一个安装位置 |
| 权威扩展名单 | `data/persons/roster.json`，JSON列表；原版850人与此扩展名单分开 |
| 头像根目录 | `data/persons/portraits/` |
| 安装目录表 | `catalog.json`，人物全名、出生年、face、folder、参考来源、批次 |
| 当前进度 | `progress.json`；全队列 `queue.json` |
| 工作说明 | `work_state.json` |
| 最新全量验证 | `verification.json`，含每人每剧本的game ID与验证范围 |
| 身份疑点 | `identity_review.json` |
| 批次材料 | `batches/0021_plan.json`、`0021.json`、`0021_installed.json`；之前批次同理 |
| 容量验证 | `build/portrait_capacity_verification.json`、`tools/patchgen/verify_portraits.py` |

每个人物文件夹通常含 `portrait_master.png`、`portrait_240x240.png/.bmp`、`portrait_64x80.png/.bmp`、`portrait_32x40.png/.bmp`。最初王猛等4人的文件名前缀不同；查询目录表及文件通配结果，不要批量重命名。参考图（找到且确认相关时）存 `references/`。

## 3. 已采用的制作方案

### 3.1 人物研究与造型

1. 从当前队列和名单选择缺头像人物，以全名、出生年和来源键核对身份、时代、职务。人物年龄可以作为美术选择，不能据此修改名单的生卒字段。
2. 查询史籍、历史画像和网络画像；优先确认画的是该人。相近姓名、同名异代及搜索配图不构成有效肖像依据。没有可信本人画像时记录“历史艺术重构”，不能声称还原本人相貌。
3. 每人使用独立提示词，脸型、年龄、胡须、表情、冠服与身份相符。保留参考网址、提示词和是否有真实/后世形象依据。
4. 美术基调：三国志11/光荣经典历史策略人物绘画，写实人体比例、手绘皮肤和衣料、克制土色、暖侧光、暗烟雾背景、上胸构图。冠帽完整，顶上约6%留白，眼睛在方图高度约40%。不得有字、水印、边框、现代摄影效果、3D、动漫脸、幻想盔甲、明代翼帽或清代服饰。
5. 中国东汉人物不要全部复用同一张老文官脸。印度、欧洲、日本等人物仍遵守各自时代文化服装与可靠肖像特征，保持同一游戏画风。

### 3.2 图像生成与落盘

- 使用内置 `image_gen`，每个不同人物一次调用。**没有用户明确授权，不切换CLI、外部API或付费模型路线**。可读取 `C:/Users/garib/.codex/skills/.system/imagegen/SKILL.md`。
- 网络研究使用 `agent-reach` 技能；本机此前没有可用的agent-reach/mcporter CLI，已说明并采用内置web搜索作为回退。技能在 `C:/Users/garib/.agents/skills/agent-reach/SKILL.md`。
- 先写批次计划和安装manifest，再生成。不要用拼贴图代替每人独立原图，不用占位图、HTML/SVG代替画像。
- 内置输出通常在 `C:/Users/garib/.codex/generated_images/01a164f2-4ed8-7a73-bbcc-fc95c3f6249a/exec-*.png`；生成后必须复制到人物工作树，不得只留生成器缓存。
- 生成结果对象可能有巨大base64内容，**不要 `text(result)` 或打印完整对象**。只提取 `output_hint` 中的 `exec-....png` 文件名。可使用 `store(key,result)`；工具持久状态不是交接依据，文件才是。
- 示例调用方式（工具代码，不是PowerShell）：

```js
// @exec: {"yield_time_ms":120000,"max_output_tokens":300}
const r = await tools.image_gen__imagegen({prompt, transparent_background:false});
store(key, r);
text({key, file:r.output_hint?.match(/exec-[a-f0-9-]+\.png/)?.[0]});
```

独立调用可放在 `Promise.all`，但后端经常逐张完成，5张需数分钟。exec返回运行cell时使用 `functions.wait` 续查同一cell；shell返回session则用 `write_stdin`。**观察超时、输出截断不是任务失败，不得仅因此重启生成**；先检查原句柄、结果元数据、生成文件及批次落盘状态。

### 3.3 导出、视觉检查、安装

以下命令均从人物工作树执行。生成完毕后，每个人运行：

```powershell
& ./tools/roster/export_portrait.ps1 `
  -Folder 'data/persons/portraits/<folder>' `
  -MasterImage 'C:/Users/garib/.codex/generated_images/<thread>/exec-<id>.png'
```

该脚本用System.Drawing高质量双三次缩放，输出24位RGB BMP与PNG。方图输出240×240；小头像64×80和32×40。对非方原图取顶部、横向居中的正方区域，避免拉伸或裁掉帽顶。小头像源区域为正方边长的x=20%、y=0、宽64%、高80%。不要再用Python重画或AI以外的图形手段修改形象；复制和格式导出不属于另做画像。

查看每人的64×80结果，确认脸可读、五官无异常、冠帽及脸没有不合理截断。必要时再看原图和240图。安装manifest格式：

```json
{"people":[{"name":"人物全名","birth":148,"face":2504,
"folder":"ascii_folder","references":["来源URL"],
"likeness":"Historical artistic reconstruction; no vetted surviving likeness located"}]}
```

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/roster/install_portrait_batch.py data/persons/portraits/batches/0022.json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python tools/roster/portrait_queue.py
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python tools/roster/verify_installed_portraits.py
```

安装器按全名+出生年检查当前名单和15个剧本，一一匹配才允许写入；检查face占用、原档案载荷及并发修改；创建回滚记录。只改选中人物的头像字段，保留其他剧本字节，安装后回读载荷及剧本。不能取消断言来掩盖身份或旧剧本缺人的问题。

`verify_installed_portraits.py` 验证目录表每人3个WFTX图像载荷与本地BMP完全一致、有效标志正确、名单face匹配、15个剧本人物唯一匹配且face正确。已改为每个剧本只扫描一次身份索引，仍保留重复匹配检查；102条时约1.4秒。它**不验证实际Direct3D画面**。

## 4. 游戏档案与扩容实现

当前游戏 `worldmod.ini` 的 `[san11pk_world]`：`faces=16000`、`persons=12613`、`data_dir=world_custom`、`save_dir=UserData_kit`、`automation=0`、`auto_skip=1`。城市530、势力42、部队1000，其他世界地图配置不要为了头像随意调整。

- 初始4张使用原档案face2196–2199：王猛、奥勒留、龙树、卑弥呼。
- 后续从face2400连续分配，当前轮次最终使用到2503；下个可用值应为2504，**恢复时仍要查catalog与实际档案**。
- 原档案：游戏 `Media/face/San11Face00.fce`。扩展档案：游戏 `UserData_kit/Koei/San11 Tc/FaceData/San11Face10.fce`。**不要把扩展档案放进Media/face**。
- 每1000个face一个扩展bank，起始2400，文件10、11……：`number=(face-2400)//1000`，`first=2400+1000*number`。不要把所有人塞进一个超过2GB的档案；原生读取存在有符号seek限制。游戏有20个档案描述符，需要同时考虑数量限制。
- FACE头20字节：magic FACE；offset4 version=64；offset8 firstFace；offset12 faceCount；offset16 imageCount=3*faceCount。随后每face4字节metadata；表起始 `20+4*faceCount`，每项 `<offset,size>` 两个uint32。
- 图像索引：大图 `localFace`；两个小图 `faceCount+2*localFace`、`faceCount+2*localFace+1`。**档案中的两个小图都是64×80**，不是32×40；32图由游戏派生/用于交付。
- WFTX头24字节，magic `WFTX0010`，总长度、图数1、u16宽高、24bpp、flags0，像素为自顶向下BGR。
- 15个剧本位于游戏 `world_custom/scenario/Scen*.s11`，大小写可能混合；以解析器定位person表。人物face为记录偏移53的int16，出生年为58的int16。
- 曾经采用过错误的64字节FACE头、颠倒offset/size和错误小图布局，已修复；不要沿用早期试验格式。

容量代码：`worldmod/src/worldmod.cpp`、`worldmod/worldmod_portraits.txt`、`worldmod/worldmod_portraits_caves.txt`、`tools/patchgen/gen_portraits.py`。生成174处字段补丁和8个code cave，动态metadata分配4F，档案manager分配24F+0x1e8。`tools/pipeline.py`已经纳入头像补丁表。`work_state.json`中capacity_findings保留早期逆向笔记，里面“Need full reconstruction”等措辞是历史记录；现已采用上文明确的格式并通过测试，不要当作尚未实现的格式。

`tools/import_roster.py`在face>=0时保留face，未分配才用通用男2000/女2100；上限32760。`tools/roster/build.py`按catalog的全名+出生年继承头像，出生年变更仍需人工核对并同步，不能只依赖继承逻辑。

## 5. 已验证的范围与尚未解决的问题

容量曾通过 `tools/patchgen/verify_portraits.py` 的Unicorn测试，历史记录为27套时356个case，覆盖F=2400/16000/32760、原始174处表达式、getter/setter、边界、年龄变体、缓存和实际档案加载。后续脚本会读取更多catalog face，case数会变化。

```powershell
python tools/patchgen/verify_portraits.py
```

还有32位原生测试 `tools/patchgen/portrait_native_check.cpp`，通过 `tools/patchgen/build_portrait_check.bat` 编译，需要VS x86工具链。它加载实际DLL并调用原生构造器/读取函数；此前三个容量和完整world补丁均通过。不要仅为增加图片反复重新部署DLL。

**运行时画面问题**：此前隐藏测试程序在Direct3DCreate9/CreateDevice附近退出，未进入剧本，出现C++异常0xe06d7363（另一次0xc0000409）。faces2400且不加载扩展bank的对照组、原扩容前DLL也复现，因此不能断言是头像扩容导致，也不能断言头像已经在屏幕上正确显示。下一步应检查显示初始化、图形设置/保存目录（原用户SysSavePK.s11存在而测试新目录可能缺失），在保留对照条件下复现。日志为 `build/portrait_runtime/worldmod_san11pk_portrait_test.log` 与 `build/portrait_runtime/worldmod_san11pk_portrait_control.log`。`tools/gamectl.py`提供内部命令与截图、激活、点击；原生CUA电脑控制在该会话不可用。

旧测试exe已清理，DLL与INI恢复。历史部署DLL SHA256：`EA08CD22324DB2E0DC2DEB0B7F5471C1CE255B45A25EC5A29248F5C8C42F011F`；恢复后如需要，重新实测hash，不能凭本文假定。

## 6. 身份、名单变更和并发处理

当前保留的身份疑点包括：李亮(108)、李敏(118)的来源时代与规范化日期冲突；卞氏(143)缺少区分同名者的充分信息；士黃有(143)可能是士䵋的编码替代。详见 `identity_review.json`，不因疑点删除人物或缩小总任务。

安清案例：名单由出生130改为129，旧的“全名+出生”继承失效。已按 `gs:gs_00:0:安清` 和 `sz:sz_00:34:安清` 核对，保留新出生129，恢复face2410，单独同步15个剧本的出生字段130→129。回滚目录 `portrait_backups/identity-sync-20261023-112751`；脚本留在 `build/sync_anqing_portrait.py`（一次性迁移，不要再次运行）。catalog记录previous_birth与source_keys。

张宴／张晏案例：两条名单记录有同一西域长史职位、同行人物、日期及攻疏勒战事，来源分别使用宴／晏。按同一历史人物异名的判断，为两条记录使用相同美术原图、不同face ID及独立文件夹；**没有删除、合并或改名名单记录**。这是依据史事一致作出的身份判断，不是发现了真实肖像。

如果全量验证失败：打印不匹配的catalog人物及当前同名记录，先检查出生年、来源键与名单是否刚更新。保留新的权威事实；不要恢复整份旧roster来“修好”头像。需要改剧本非face字段时单独备份、断言旧值、只改确定字段，并重新验证。

## 7. 备份与回滚

- 原始首轮备份：游戏 `portrait_backups/20261023-030535`（原头像档案、15剧本、名单）。
- 扩容前DLL/INI：`portrait_backups/capacity-20261023-032437`。
- 每批安装：`portrait_backups/batch-YYYYMMDD-HHMMSS/rollback.json`；准确目录见相应 `NNNN_installed.json`。记录档案原长度/哈希/修改字节及剧本字段旧值。
- 回滚前检查当前文件是否已包含后续批次或并发更改。**不能直接截断整个bank到早期长度或覆盖整份剧本，把后来的头像与名单修改一起抹掉**。
- `build/portrait_native`中存在指向world_custom等位置的junction；不要递归删除该树。Windows删除/移动前确认绝对路径和链接边界，操作使用LiteralPath。

## 8. 接手与完成验收

用户说“继续/恢复”后：

1. 在人物工作树读取progress、catalog、work_state、identity_review、最新批次installed报告，重新运行queue及全量verify；检查是否有未完成的生成句柄或已有图片，避免重复生成。
2. 根据当前名单选择下一批缺头像人物；当前轮次完成后预计下一批为0022、face2504起。队列可能包含早期身份待核对项，不要只按旧ID接着加。
3. 按第3节研究、保存计划、逐人生成、导出、查看小头像、安装、回读。持续向用户报告真实进展。
4. 同步可变进度、身份疑点和备份位置。遇到剧本不存在的新人物，要完成最新名单迁移，不能跳过应用要求。
5. 单独解决运行时显示验证，并完成全部人数覆盖后再做最终验收。

最终完成需要同时成立：当前权威名单中的每个需头像人物都有可用资产与可靠映射；所有目标剧本/武将确实可用这些头像；没有遗漏身份待处理项；档案结构/容量/载荷/全部人物绑定校验通过；实际游戏画面能够显示并正确裁切；最新名单完整部署。只有这些都有证据才可将goal置complete。此次应置paused，用户恢复前不再生成或写入游戏。

## 9. 可直接给接手助手的提示词

> 读取本仓库PORTRAITS_HANDOFF.md和data/persons/portraits中的work_state.json、progress.json、verification.json。目标仍是给当前全部缺头像的扩展武将逐人制作三国志11风格头像并应用于游戏。先检查最新名单及上一批落盘结果，保留并发更改；按身份而非旧ID匹配。使用内置image_gen，不自行切换API。不要把文件校验当成游戏画面验收；游戏显示问题及最新名单到旧剧本的完整迁移仍须解决。用户已经授权这些制作与游戏写入；只在用户明确恢复后继续。每轮保存计划、参考来源、原图、三尺寸导出、安装报告和回读证据。
