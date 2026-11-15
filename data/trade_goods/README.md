# 贸易商品图片

现有 72 个商品图卡，其中 71 张图片来自《大航海时代 Online》，原始来源、下载地址、尺寸和 SHA-256 见 [sources.json](sources.json)。保留原图格式与像素；来源记录不代表取得再分发或商业授权。

将本目录复制到游戏 `data_dir` 下的 `trade_goods` 子目录，默认是 `world_custom/trade_goods`。更新图片后重启游戏。图片首次打开贸易窗口时解码并缓存；缺图显示“暫無圖片”。

| 游戏商品 | 图片文件 | 原作商品 |
| --- | --- | --- |
| 絲 | silk.gif | 蠶絲 |
| 蜀錦 | brocade.gif | 中國錦緞（近似） |
| 漆器 | lacquer.jpg | 漆器 |
| 鐵器 | iron.gif | 鐵材（原料替代） |
| 馬 | horse.jpg | 馬 |
| 玉 | jade.jpg | 和田玉 |
| 胡椒 | pepper.jpg | 胡椒 |
| 棉布 | cotton.jpg | 棉布料 |
| 玻璃 | glass.jpg | 玻璃工藝品 |
| 珊瑚 | coral.jpg | 珊瑚 |
| 乳香 | incense.gif | 乳香 |
| 皮毛 | fur.gif | 毛皮 |
| 珍珠 | pearl.jpg | 珍珠 |
| 鹽 | salt.gif | 鹽 |

城市贸易的行情、商品选择、货物与交易明细使用商品行预览；各地行情、组队设置、数量输入与交易确认保留当前商品图。列表排序使用原生行的稳定标识匹配图片。

2026-11-02 新增商品的完整映射见 `sources.json`，保留原作名称与近似替代说明。大黄未找到可核实原图，记录为 `status=missing`，正常显示「暫無圖片」。运行 `python tools/trade_goods_images.py` 校验现有文件哈希并重新生成 `worldmod/src/trade_goods_images.inc`。

新增槍、戟、弩分别使用 Online 的長槍、戟、硬弩图片；前两项取手持装备图，后一项取交易品图，均为同类示意，不表示汉末器型复原。
