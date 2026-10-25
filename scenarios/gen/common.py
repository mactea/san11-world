import json
ROME_PROV = ["羅馬", "米蘭", "阿奎", "拿坡", "敘拉", "塔拉", "科爾", "梅里", "里昂", "納博", "馬賽", "波爾", "蘭斯", "特里", "倫丁", "約克", "美因", "奧格", "卡農", "阿昆", "西爾", "薩洛", "薩米", "塞爾", "拜占", "雅典", "帖撒", "以弗", "安卡", "馬薩", "塔爾", "安都", "大馬", "帕爾", "耶城", "尼西", "黎軒", "孟菲", "底比", "迦太", "錫爾", "約爾", "丁吉", "萊普", "昔蘭", "拉文", "布林", "巴勒", "都靈", "佛羅", "卡利", "新迦", "加的", "薩拉", "里斯", "布拉", "阿斯", "巴黎", "普瓦", "貝桑", "斯特", "切斯", "卡迪", "雷根", "維魯", "都拉", "納伊", "菲利", "哈德", "馬爾", "托米", "奧比", "克森", "克里", "羅德", "塞浦", "錫諾", "特拉", "阿塔", "科尼", "梅利", "佩特", "埃米", "杜拉", "佩魯", "賽伊", "希波", "沃呂", "安科", "雷焦", "阿萊", "盧戈", "托萊", "瓦倫", "克盧", "潘普", "貝哈", "科英", "帕馬", "奧爾", "利摩", "克萊", "魯昂", "雷恩", "南特", "布洛", "弗雷", "通格", "卡萊", "溫徹", "桑滕", "奧古", "帕紹", "波埃", "穆爾", "因斯", "庫爾", "維米", "波洛", "羅穆", "佩林", "帕特", "阿馬", "塞巴", "特洛", "安提", "托勒", "貝列", "錫瓦", "塞蒂", "波馬", "塔卡", "貝倫", "蘇菲", "法西", "赫格"]
# 2026-10-26 user: the Roman force keeps only its core, 20 cities (Italy, the islands, Dalmatia, Noricum,
# Narbonensis, Africa); the other provinces stay unowned, their officers move to the nearest core city
ROME_CORE = ["羅馬", "米蘭", "阿奎", "拿坡", "拉文", "布林", "都靈", "佛羅", "安科", "雷焦", "敘拉", "巴勒", "卡利", "阿萊",
             "薩洛", "維魯", "馬賽", "納博", "迦太", "萊普"]
ROME_CITIES = [{"city": "羅馬", "troops": 20000, "gold": 4000, "food": 40000} if c == "羅馬" else c for c in ROME_CORE]
ROME_AUTO = {"polity": ["羅馬帝國"], "roles": ["武将", "文臣", "宗室"], "min_conf": "B",
             # the roster's estimated years let in a few whose careers lie decades before / after; and ladies without office
             "exclude": ["Regalianus", "Ragonius Venustus", "Publius Cornelius Saecularis", "Marcus Nummius Albinus",
                         "Marcus Pupienus Africanus Maximus", "Gnaeus Domitius Philippus", "Gaius Macrinius Decianus",
                         "Macedo (Osroene commander)", "Marcus Bassaeus Rufus", "Quintus Baienus Blassianus",
                         "Didia Clara", "Annia Cornificia Faustina Minor", "Vibia Aurelia Sabina"]}

def O(p, city=None, **kw):
    d = {"p": p}
    if city: d["city"] = city
    d.update(kw)
    return d

def dump(spec, path):
    s = json.dumps(spec, ensure_ascii=False, indent=1)
    open(path, 'w', encoding='utf-8', newline='\n').write(s + '\n')

# city resources by tier (data/city_tiers.json) about the level of Koei's Chinese cities, and no new force richer
# (troops, gold, food summed over its cities) than the richest Koei force of the same scenario
RESOURCES = {"tiers": {"1": {"troops": 15000, "gold": 3000, "food": 30000},
                       "2": {"troops": 8000, "gold": 1600, "food": 16000},
                       "3": {"troops": 3000, "gold": 800, "food": 6000}},
             "force_cap": "strongest_koei"}
