# 商品图片接入验证（2026-11-01）

城市原生贸易列表增加 88 像素预览区，卡片为 320×80。商品图片按名称匹配；列表排序后通过原生行记录的稳定键取图。数量输入使用独立预览，关闭后清除。GDI+ 解码延迟到首次绘制，避开 DllMain loader lock；D3D9 纹理缓存并在设备重置时释放，绘制前后保存恢复渲染状态。

## 验证

- 独立 trade-game，测试存档 10，195 年 8 月 11 日刘备小沛。日志确认图片加载 14/14。
- 行情点击玉、按卖出价排序后点击马，均显示正确图片；返回市场和城市菜单无残留。
- 组队选择目的地和丝，组队表、数量键盘、出发确认均显示丝图；返回取消，无实际出发或保存。
- 数量键盘图片不遮挡输入及决定/返回按钮。截图位于 `worldmod/build/trade-game/pictures-quantity.bmp`。
- DLL 与测试程序重新编译通过；trade_runtime_test、trade_regression_test、pathfind_route_test、trade_panel_test 全部零失败。运行时测试覆盖稳定行键、禁用商品行与无焦点回退。
- 14 张源图 SHA-256 全部匹配 sources.json；git diff --check 通过。
- 尚未逐一实机验证货舱、近期交易分支，也未切换低分辨率验证数量窗口的上方回退布局。

## 部署

已更新正式游戏 `C:\Games\san11\Romance Of Three Kingdom 11 PK` 的 d3d9.dll 和 world_custom/trade_goods；部署时正式游戏未运行。原文件备份至 `trade-backups/20261101-143852-commodity-pictures`。

DLL SHA-256：`98148A358A5C48ACC63A266DD0120A944F2D5F6B9BD2C3E44E1428D06D4A16F9`。

配置与正式存档共 7 个文件部署前后哈希一致；未修改存档。图片来源与近似对应说明见 `data/trade_goods/README.md` 和 `sources.json`。
