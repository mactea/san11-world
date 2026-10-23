# 从公版/开放史料抽取人物（子代理用）

字段、译名、五维、输出格式**完全照 `data/persons/raw/SPEC_CLASSIC.md`**（先读它），本文件只写各书的特别规定。
块首一行写着书名。只收活动时间与 **公元 170–370 年** 有交集的真实个人；查重用 `data/persons/_cache/known_world.tsv`
（Grep 英文/拉丁/法文写法的词根，例如 Tiridate → Tirid、Nersès → Ners）；已有的跳过。

## fau 拜占庭的浮士德《亚美尼亚史》（法译）
* 4 世纪亚美尼亚：国王（Diran=Tiran、Arschag=Arshak II、Bab=Pap、Varazdat）、王后（Pharantzem）、
  宗主教（Nersès、Housig、Daniel）、纳哈拉尔（Mamikonian 家的 Vasag、Mouschegh、Manuel，Kamsarakan、Bagratouni……）、
  波斯王与将领、罗马将领。`name_en` 用英文通行写法（Arshak II、Vasak Mamikonian、Mushegh Mamikonian）。
* polity：亞美尼亞（阿沙克王朝）/ 亞美尼亞教會 / 薩珊波斯 / 羅馬帝國；src 写 `Faustus 4.15`（卷.章）。
* 书里的神迹、圣人传说照录事迹即可，人是真的就收；纯传说人物不收。

## moi 霍伦的摩西《亚美尼亚史》卷 2–3（法译）
* 成书晚（5–8 世纪），卷 2 前半是传说。**只收 170 年以后的人**（Vologèse/Valarsh 之后、Khosrov、Tiridate、Grégoire、
  Arschag、Bab……）。和浮士德重复的人以浮士德为准，本书只补浮士德没有的。拿不准真假的写 `"conf": "C"`。src `Moses 2.70`。

## mhv《大史》33–37 章、《岛史》21–22 章（英译）
* 斯里兰卡阿努拉德普勒诸王（Vohārika Tissa、Abhayanāga、Siri Nāga、Goṭhābhaya、Jeṭṭha Tissa、Mahāsena、Sirimeghavaṇṇa）、
  王后、王子、大臣、僧人（Saṅghamitta 等）。只收在位/活动与 170–370 交集的。年份用通行的 Geiger 年表（例如 Mahāsena 274–301）。
* polity：阿努拉德普勒王國 / 錫蘭佛教；place 阿努拉德普勒（8.35, 80.40）。src `Mahavamsa 36.5` / `Dipavamsa 22.40`。

## niya 尼雅佉卢文书（Burrow 英译）
* 3–4 世纪鄯善（Kroraina）。收：国王（Pepiya、Tajaka、Amgoka、Mahiri、Vasmana）、王后、王子、
  有官职且出现多次的官员（cozbo、kitsaitsa、ogu、tomga、sothamga、vasu、ageta……，例如 cozbo Somjaka、Tamjaka、Bhimasena）、
  有名有事的使者、僧人。**不收**只在一份契约/诉讼里出现一次的普通人、奴隶、证人名单。
* 年份：按 Brough 年表估（Amgoka 约 250–290 在位，Mahiri 约 290–318，Vasmana 约 318–330；不确定就给 [250, 320] 这种范围）。
* polity：鄯善；place 精絶（尼雅 37.98, 82.72）或 扜泥（且末/鄯善都城 39.0, 88.0 附近，按文书写的地方）。
  src `Niya Kh. 571`（文书号）。bio 写他在文书里做了什么（审案、收税、被告……）。conf：多次出现 B，只一两次 C。

## cth《狄奥多西法典》受文人
* 每行是一条法令：编号 | 开头（发布皇帝 + 受文人 + 官职）|| 日期（地点 + 执政官年）。
* 收：**受文人**（praefectus praetorio=禁衛軍長官、praefectus urbi=城市長官、vicarius、proconsul、comes、
  magister、praeses、consularis、corrector……）和日期里出现的**执政官**本人（非皇帝的）。
  只收法令年份 ≤ 375 的（按执政官推算年份；括号里编者给了年份就用它）。皇帝、「ad populum / ad senatum / ad provinciales」不收。
* 同一人多条法令合并成一条：offices 列出所有官职，years 取最早、最晚法令年份。bio 写「某年任某官，受某帝某某敕令」这样的事实。
* polity 羅馬帝國；place 按官职驻地（城市長官→羅馬/君士坦丁堡，阿非利加總督→迦太基，東方禁衛軍長官→君士坦丁堡/安條克）。
  src `CTh 1.2.3`。五维：一般 统率 20–45，政治 55–75。conf B。

## soc / soz / thd / phs / vc 教会史家（希腊文）
* 苏格拉底、索佐门、狄奥多勒、腓罗斯托尔吉乌斯《教会史》、优西比乌《君士坦丁传》。收 170–370 年活动的主教、教士、修士、
  殉道者、异端领袖（阿里乌派、梅勒提乌派、多纳图派……）、官员、将领、皇族、外族君主（哥特、波斯、亚美尼亚、埃塞俄比亚、伊比利亚）。
  **只收 370 年以前已经开始活动的人**（370 年以后才当主教的不收）。
* 这几部书的人大多已经收过（优西比乌、阿米阿努斯、佐西默斯、维基）——一定先查 known_world.tsv，常见写法：
  Athanasius、Arius、Eusebius of Nicomedia、Acacius of Caesarea、Eudoxius、Macedonius、Aetius、Eunomius、Meletius……
* polity：羅馬教會（正统派）/ 阿里烏派 / 羅馬帝國 等；src `Socrates 2.15` / `Sozomen 3.4` / `Theodoret 1.7` /
  `Philostorgius 3.2` / `Vita Constantini 4.7`。不同书里的同一人用同一个 name_en（维基英文条目名）。

## c354 罗马城历书（354 年）
* 执政官表（「A et B」是当年两位执政官的 cognomen，夺格；编者注里常有全名）、罗马城市长官表（254–354，每任有起止日期）、
  教宗与殉道者的纪念日、利伯里乌斯教宗表、罗马城编年（各帝在位年数、事迹）。
* 收非皇帝的执政官、城市长官、教宗、有名有日期的殉道者。执政官只有 cognomen、认不出是谁的写 `"conf": "C"`，
  name_en 用通行写法（如 Rufius Volusianus）。src `Chronography 354, Prefects 310`。官员五维：政治 50–70，其他低。

## eun 欧纳皮乌斯《智者传》
* 3–4 世纪新柏拉图派哲学家、修辞家、医生：普罗提诺、波菲利、扬布里柯、埃德西乌斯、马克西穆斯、普赖赫雷西乌斯、利巴尼乌斯……
  以及他们的弟子、妻女（索西帕特拉）。只收 370 年以前已经活动的。role 学者；polity 羅馬帝國。src `Eunapius VS 461`。

## jor 约达尼斯《哥特史》
* 6 世纪书，追述 3–4 世纪哥特人：奥斯特罗哥塔、克尼瓦、阿里亚里克、盖贝里克、埃尔马纳里克、维西马尔、维提米里斯，汪达尔王，
  罗马方面的皇帝将领。**哥特王族世系（阿马尔家谱）里只有名字、没有事迹的不收**；事迹带传说色彩的写 `"conf": "C"`。
  polity 哥特 / 東哥特 / 西哥特 / 汪達爾 / 格皮德；src `Jordanes Getica 16.90`。

## lat 奥勒留·维克托、欧特罗皮乌斯卷 8–10、拉克坦提乌斯《迫害者之死》（拉丁文）
* 2–4 世纪皇帝、篡位者、将领、禁卫军长官、总督；人大多已收，先查表。src `Aurelius Victor 39.20` / `Eutropius 9.22` /
  `Lactantius DMP 12`。

## je 犹太百科「Amora」「Tannaim and Amoraim」
* 收列表里 170–370 年活动的坦拿（最后一代）和阿摩拉（第 1–5 代）：巴比伦（Rav、Samuel、Rav Huna、Rav Judah、Rabbah、
  Rav Joseph、Abaye、Rava……）和巴勒斯坦（Judah ha-Nasi、Johanan bar Nappaha、Resh Lakish、Ammi、Assi……）。
* 年份按代估：第 1 代约 220–250，第 2 代 250–290，第 3 代 290–320，第 4 代 320–350，第 5 代 350–375（文里写了卒年就用）。
* polity：猶太（巴比倫）/ 猶太（巴勒斯坦）；place 苏拉、蓬贝迪塔、马胡扎、太巴列、塞佛瑞斯、凯撒利亚（按学院）；
  role 学者；src `JE Amora`。师承写进 friends（老师/学生），父子写进 kin。
