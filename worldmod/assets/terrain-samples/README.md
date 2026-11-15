# 地形重绘样板（未部署）

## 22:59 加载器身份修正

本轮没有生成或修改图片，已有接缝/风格问题仍未验收。`replacement-v2.wmtr` 保留为合成测试夹具；`prepare_ground_identities.py` 新建 `replacement-loader-profiles.wmtr`，按原加载器代码生成 X8R8G8B8/单级、64² 或 32² 的源身份，覆盖春夏秋共用和冬季。半尺寸按 2×2 各通道之和向下取整除 4，目标仍使用上述两个候选的 1024²/11 级 mip。源数据直接读取原 LINK/WFT，不依赖编辑后的 PNG。

四种身份的独立双卡对照通过，但仅覆盖未开启 DXT1 转换且支持 X8R8G8B8 的加载分支；不是游戏运行时捕获，包未部署。详见 [离线交付](../../MIGRATION_OFFLINE.md)。

## 后续版本与已核实用途

新增 `ground-12c0-01-v2.png` 和 `ground-12c3-01-v1.png`，均使用内置 `image_gen`；原始输出复制到本目录，未覆盖 v1。资源清点确认索引 1 的春/夏/秋原始像素完全相同，冬季不同，因此新包包含两个内容身份。

进一步核对 `TERRAIN_WINDOW.md`、当前 EXE 和本地地图数据后，已确认层号直接索引 36 字节纹理记录，UV 重复周期取原尺寸/32。这张 64² 原图对应 2×2 顶点周期；原生替换保持原几何 UV，不让 1024² 新纹理把世界中的重复周期放大。

按 `tools/make_world.py::learn_materials` 的最近六角格坐标方法，原版中国地图中材质 1 的 3,437 个关联顶点约 **92.87% 位于毒泉类型**。这是统计关联，不是每个顶点的确定地形标签。此前“土壤样板”是外观描述，不能视为通用草地/道路素材。新的石土细节是否适合这一用途尚未验收。

v2 的平均 RGB 从 v1 的 `(97.55,92.94,76.49)` 改为 `(102.00,98.99,81.74)`，更接近原图 `(100.14,100.13,85.32)`；但横/纵接边差为 16.53/17.18，接缝指标比 v1 更差。冬季版本平均约 `(98.35,98.23,94.77)`，原图为 `(95.79,95.73,91.70)`；重复拼贴和局部石子高光仍需修订。**这两个版本保留为技术候选，不宣称无缝或美术验收通过。**

`pack_terrain_replacement.py` 构建两个条目的 `WMTR0002` 包：每张目标 1024²、11 级 mip。当前构建使用解码原图的 A8R8G8B8/BOX mip 身份；原游戏可选 DXT1 转换，故尚未证明此身份与游戏实际纹理一致。精确匹配失败时不会替换。生产路径和压缩身份均已用合成独立程序验证，不能据此宣称游戏中已命中。

v2 生成源：`C:/Users/garib/.codex/generated_images/01a1956d-0007-7b60-8869-47ee7de3b4dd/exec-bc19a9d3-2d23-4c63-a0b3-724e3c4171a5.png`。完整提示词：

> Use case: style-transfer. Edit target: image 2, the detailed soil texture sample. Reference image 1 is the original game terrain albedo and controls palette and broad structure. Produce one revised square tileable ground diffuse texture. Correct the sample's overly warm brown and dark palette toward the original muted neutral grey olive, average RGB approximately 100,100,85. Reduce large directional streaks and obvious repetitive clusters. Preserve subtle soil and fine mineral detail, low contrast. Make opposite edges join visually seamlessly in both directions, no border and no identifiable central feature. Strict top-down orthographic flat albedo, no lighting gradient, shadows, AO, perspective, objects, grass blades, text, annotations, grid, or collage. Single opaque texture only, 1024x1024 requested. This is a revision of one asset, not a new scene or season.

冬季生成源：`C:/Users/garib/.codex/generated_images/01a1956d-0007-7b60-8869-47ee7de3b4dd/exec-6a6d1185-56bc-4979-98dc-6ab500cc101c.png`。完整提示词：

> Use case: lighting-weather. Image 1 is the edit target, a single game terrain tile; image 2 is the original winter texture and is the color reference. Produce the winter counterpart of image 1, preserving the same soil/mineral positions, scale and low contrast but matching the original winter reference's neutral grey muted palette and brightness. Remove the yellow olive cast without inventing white snow, ice, vegetation, objects or new large features. Flat diffuse albedo, top-down orthographic, no directional lighting, no text, borders or collage. Opposite edges must join naturally for a repeating tile on both axes. Single square 1024x1024 texture requested. Keep the same spatial structure for smooth season changes.

## v1 历史说明

本样板对应已核对加载表的秋季资源 `0x12C0`、图像索引 `1`（从 0 开始）。原图为 64×64、24 位 BGR；具体地形类型、UV 使用和季节间材质关联尚未确认，不能仅凭外观命名为道路或草地。

使用内置 `image_gen`，以解码原图为参考生成 `ground-12c0-01-v1.png`。请求 1024×1024，工具实际返回 **1254×1254**；此文件保留工具原始输出。源输出位于 `C:/Users/garib/.codex/generated_images/01a1956d-0007-7b60-8869-47ee7de3b4dd/exec-f0226663-66a2-42c7-a94e-9651e95b7e25.png`。

完整生成提示词：

> Use case: style-transfer. Asset type: a single square terrain diffuse texture sample for a historical strategy game. Edit target: the provided 64x64 decoded original ground texture (resource 0x12c0 image 1). Redraw it at 1024x1024 with subtle readable mineral/soil and very sparse moss detail while preserving its muted grey olive brown palette, low contrast, evenly distributed mottling, and overall average brightness. Strict orthographic top down flat albedo texture, no perspective, no horizon, no large identifiable objects, no grass blades, no cast shadows or directional highlights, no baked ambient occlusion, no text, no border. Tileable seamlessly on both axes; opposite edges must meet without a seam, avoid central features. Do not sharpen the original pixel grid or add flashy saturation. This is one experimental sample, not an entire atlas. Opaque background.

## 构建和验证

在仓库根目录运行 `python worldmod/tests/prepare_terrain_sample.py`，仅写入 `worldmod/build/terrain-assets/`。先读取本机原资源并验证加载表，然后将生成图缩至 1024²，生成 11 级 mip 至 1²；产物为 A8R8G8B8、BGRA 字节序，总载荷 **5,592,404 字节（约 5.33 MiB）**。这不含 D3D12 分配对齐、原 D3D9 副本、上传区和 CPU 副本，不能直接当成整体显存成本。mip 采用现有非 sRGB 路径的编码通道平均；不引入新的色彩空间处理。

`run_render_tests.ps1` 自动构建样板，并在两块显卡上验证原 D3D9 与原生 DX12 的四种 UV 缩放和三轮 Reset：每卡 12 组像素对照。该检查验证格式、mip 和渲染一致性，不证明美术风格或游戏画面验收通过。

## QA 结论与接入边界

已检查原图、重绘图和 2×2 重复拼贴。样板增加了细碎石土纹理，但比原图偏暖、略暗；重复拼贴仍有可辨识的纹理分布。原图平均 RGB 约 `(100.14,100.13,85.32)`，生成图约 `(97.55,92.94,76.49)`。相对边缘平均差约 13.22/13.64，高于相邻内部像素的 8.99/9.55；相对边缘不完全相同，**尚未通过无缝与风格验收**。不能因提示词要求可平铺就宣称无缝已完成。

下一步先确认实际 UV/地形用途、匹配其他季节并修订风格和接缝，再决定加载替换方式与内存预算。当前只有这一张秋季样板，未替换游戏资源包，未实现生产素材替换器，不批量放大整套素材。对比图及机器可读尺寸、哈希、边缘指标见 `build/terrain-assets/sample-v1/`。
