# 汉风双桅海贸船：完整 3D 外观资产

当前资产（2026-11-10）：21 个命名部件、7,492 个三角面、22,476 个顶点，Y 轴向上。保留宽厚船体和 1.5 倍建模缩放基准，采用一高一低双桅、每桅一面帆、低矮前篷、中部货篷及略高的单层舵舱，保留货舱口、侧走道、货物、桨架和系泊细节。去掉前一版的欧洲式高艉楼、窗廊、层叠顶帆、三角后帆、艏斜桅及索梯。木色舱篷和米白帆使整体更朴素。

这是参考汉代船模特征的游戏美术复原，双桅帆装和大型海船比例含推测，不是考古确定的标准船型。已通过几何、原生 D3D9 绘制和绑定集成测试；新造型在完整游戏中的靠港、选取、阴影和密集船队帧率尚未实测。一个绘制批次、256×16 贴图，面数较三桅版减少约 30%。模型和贴图已更新正式游戏，保存并重启后加载，当前进程和存档未修改。对比图和部署记录位于 `../../../outputs/trade-ship-han/`。旧版历史记录如下。

2026-11-10：专属海运商船相对原模型整体放大至 1.5 倍，船长、船宽和高度增加 50%。缩放烘焙进所有导出格式的顶点，以原点／水线为基准；骨架、法线、UV、贴图及面数保持一致。`tools/build_trade_ship_model.py` 的 `VISUAL_SCALE=1.5` 是重建基准，不修改普通船资源或商队容量。正式模型文件已更新，当前运行进程需重启后加载。离线几何、原生 D3D9 绘制及绑定集成测试通过；新版游戏场景中的选取、靠港与密集船队效果尚未实测。

同镜头尺寸对比：`../../../outputs/trade-ship-larger/comparison.png`；本次测试与部署记录：`../../../outputs/trade-ship-larger/`。下文原有实机检查记录对应放大前版本。

三桅远洋版是本轮之前的设计，因造型含后世欧洲帆船特征而被当前双桅版替代，历史记录在 `../../../outputs/trade-ship-ocean/`。所有版本货舱均以外部舱口/舱盖表现，没有可进入的内部空间；帆和舵当前是静态造型。

## 查看和编辑

- `preview.html`：双击在浏览器打开；拖动旋转、滚轮缩放、双击复位、按钮自动旋转。完全离线，不依赖 CDN。
- `preview.png`：真实网格渲染的四视角检查图。
- `merchant_ship.glb`：自包含 glTF 2.0，保留部件名和材质，可导入支持 glTF 的 3D 软件。
- `merchant_ship.obj` / `.mtl`：通用编辑格式，两个文件保持同一目录。
- `merchant_ship.wkm` / `.wft`：游戏原生格式候选资产。
- `native-d3d9.png`：原生加载器上传顶点/索引后，独立 D3D9 测试实际绘制的截图；不是游戏内截图。
- `validation.json`：文件 SHA256 和跨格式几何校验结果。

## 已验证与尚未完成

已验证 GLB 容器、法线/绕序、非退化几何、包围盒、原生顶点与索引边界、贴图布局、OBJ 面数，以及 GLB/WKMD 的几何一致性。游戏原生模型绑定、GPU 上传、GPU 资源释放成功；使用相同原生顶点格式、索引和 WFT 调色板的独立 D3D9 绘制成功。WebGL 预览已在本机 Edge 离屏渲染中检查。

**已实现贸易船独立外观绑定，在隔离游戏中实际绘制，并于 2026-11-06 部署到正式游戏。** 新船型使用本模型，普通船仍使用原生资源。已检查两个视角、旧存档载入、单位选择、移动预览及一格移动/转向；修复原生轮廓覆盖双面船帆的问题。长时间航行、到港返航、完整游戏设备重置和密集船队性能仍待验证。详细证据、备份位置和安装方式见 `../../../docs/贸易船3D外观.md`。

原生导出从用户本机走舸资源保留骨骼/格式元数据，所有船体顶点和调色板重新生成。重建需要用户自己的游戏资源文件。GLB/OBJ 不依赖该元数据。

## 重建

在项目根目录执行（Python 需 numpy、Pillow；原生测试需 MSVC x86 与 Windows SDK）：

```powershell
python tools/build_trade_ship_model.py --out data/trade_ships/merchant --game "C:/Games/san11/Romance Of Three Kingdom 11 PK"
python tools/preview_trade_ship.py --out data/trade_ships/merchant
python tools/validate_trade_ship_model.py data/trade_ships/merchant
cmd /c worldmod\test\build_trade_ship_asset.bat "C:\Games\san11\Romance Of Three Kingdom 11 PK"
```

不提供 `--game` 时只导出通用编辑格式。原生测试创建隐藏 D3D9 窗口，仅执行复制到测试进程的模型函数，不运行游戏入口、不附加当前游戏进程；GPU 回读图输出为 `native-d3d9.bmp`。
