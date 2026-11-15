# 贸易数据兼容夹具

`trade-v4-14.bin` 是本次扩容前已部署的 530 城、14 商品 TRDB v4 数据，由本项目配置生成，不包含游戏存档或程序。SHA-256：`6a8bfc79a5f8ecaf09a076d95f2ee15a7826449e422ae1d05f57c489eed86242`。

用于验证旧数据读取、全部原商品的城市价格／供给／需求／载重不变，以及 TRAD v8/v9 从 14 商品迁移到 69 商品。不能用新目录重新生成并覆盖，否则会失去独立的兼容性基准。

从仓库根目录执行：

```powershell
python tools/trade_data.py --out worldmod/build/trade.bin
python tools/trade_goods_images.py
cmd /c worldmod/test/build_sim.bat
worldmod/build/trade_runtime_test.exe
```

runtime 的可选参数：`旧 trade.bin 新 trade.bin 提取的 TRAD 块 年 月 日`。它仅在进程内迁移并验证原字段逐字节保留，不读取或改写原始 `.s11` 存档。真实存档提取块和验证日志放在忽略的构建目录，不能纳入版本库。

`trade-v5-69.bin` 为追加枪、戟、弩前的已部署 69 商品目录（SHA-256：`9669372cf2a51d3c9b83c84db02c8c652c0950a9003f17630fe1d7b8a433b319`）。当前默认 runtime 同时验证 14→72 与 69→72；其余旧兼容夹具保持原样。

`trade-v5-72.bin` 是军需价格校准前的 72 商品目录，用于验证仅马、枪、戟、弩价格变化，其他 68 商品价格及全部供需、载重、编号不变。当前数据生成格式为 TRDB v6，存档仍为 TRAD v9。价格检查命令：`worldmod/build/trade_pricing_test.exe worldmod/test/fixtures/trade-v5-72.bin worldmod/build/trade.bin`。
