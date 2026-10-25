import sys, copy, json
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import *
# start from the 198 forces that did not change, then re-check every ruler for 194
spec198 = json.load(open(sys.argv[2], encoding='utf-8'))
by = {f['name']: f for f in spec198['forces']}

NIGER = ["安都", "塔爾", "拜占", "黎軒", "大馬"]
EAST_UNOWNED = ["帕爾", "埃米", "杜拉", "耶城", "孟菲", "底比", "佩魯", "賽伊", "安提", "托勒", "貝列", "錫瓦", "佩特", "赫格", "塞浦", "尼西"]
ALBINUS = ["倫丁", "約克", "切斯", "卡萊", "溫徹"]
BRIT_UNOWNED = ["卡迪"]
sev = [c for c in ROME_PROV if c not in NIGER + EAST_UNOWNED + ALBINUS + BRIT_UNOWNED]
sev = [{"city": "羅馬", "troops": 30000, "gold": 10000, "food": 80000} if c == "羅馬" else
       {"city": "佩林", "troops": 40000, "gold": 6000, "food": 60000} if c == "佩林" else
       {"city": "科尼", "troops": 30000} if c == "科尼" else c for c in sev]
F = []
F.append({"name": "羅馬", "ruler": "Septimius Severus", "stars": 1,
  "intro": "193年佩蒂納克斯遇弒後，潘諾尼亞總督塞普提米烏斯·塞維魯稱帝入羅馬，以不列顛的阿爾比努斯為凱撒，率軍東討敘利亞的尼格爾。194年初其將坎迪杜斯已在基齊庫斯、尼西亞兩敗尼格爾，阿努利努斯正向奇里乞亞進軍。",
  "cities": sev,
  "officers": [O("Septimius Severus", "佩林"), O("Julia Domna", "佩林"), O("Gaius Fulvius Plautianus", "佩林"),
               O("Publius Cornelius Anullinus", "科尼"), O("Valerianus (general of Severus)", "科尼"),
               O("Tiberius Claudius Candidus", "科尼"), O("Lucius Fabius Cilo", "安卡"),
               O("Cassius Dio", "羅馬"), O("Aemilius Papinianus", "羅馬"), O("Julia Maesa", "羅馬"), O("Julius Avitus", "羅馬")],
  "auto": ROME_AUTO,
  "relations": {"尼格爾": 0, "阿爾比努斯": 80, "安息": 25, "哈特拉": 25, "奧斯若恩": 20, "博斯普魯斯": 85, "亞美尼亞": 50,
                "伊比利亞": 55, "喀里多尼亞": 25, "夸迪": 55, "阿克蘇姆": 60, "庫施": 50, "貴霜": 60}})
F.append({"name": "尼格爾", "kokugo": False, "ruler": "Pescennius Niger", "max_cities": 5, "stars": 3,
  "intro": "敘利亞總督佩斯切尼烏斯·尼格爾，193年在安提阿被東方軍團擁立為帝，據有敘利亞、埃及與小亞細亞。193年末至194年初敗於基齊庫斯、尼西亞，退守陶魯斯山口；拜占庭仍為其堅守。帕提亞與哈特拉遣兵相助。",
  "cities": [{"city": "安都", "troops": 30000}, "塔爾", "拜占", "黎軒", "大馬"],
  "officers": [O("Pescennius Niger", "安都"), O("Aurelianus (adviser of Pescennius Niger)", "安都"), O("Antistius Capella", "拜占")],
  "relations": {"阿爾比努斯": 30, "安息": 70, "哈特拉": 75}})
F.append({"name": "阿爾比努斯", "kokugo": False, "ruler": "Clodius Albinus", "max_cities": 5, "stars": 4,
  "intro": "不列顛總督克洛狄烏斯·阿爾比努斯，193年受塞維魯封為凱撒，名義上與之結盟，擁不列顛三個軍團坐觀東方戰事。",
  "cities": ALBINUS,
  "officers": [O("Clodius Albinus", "倫丁")],
  "relations": {"喀里多尼亞": 25}})
p = copy.deepcopy(by["安息"])
p["intro"] = "帕提亞國王沃洛吉斯五世，191年即位。羅馬內戰中支持尼格爾，奧斯若恩、阿迪亞貝尼諸王乘機起兵圍攻羅馬駐守的尼西比斯。"
p["officers"] = [O("Vologases V", "斯賓"), O("Vologases VI", "斯賓", birth=175)]
p["relations"].update({"奧斯若恩": 60})
F.append(p)
F += [copy.deepcopy(by[n]) for n in ("步度根", "軻比能", "素利")]
for f in F[-3:]:
    f["relations"]["李傕"] = 40
a = copy.deepcopy(by["亞美尼亞"]); a["intro"] = "亞美尼亞阿薩息斯王朝國王庫思老一世，約191年即位。羅馬內戰中保持中立。"; F.append(a)
o = copy.deepcopy(by["奧斯若恩"])
o["intro"] = "埃德薩的阿布加爾八世。194年羅馬內戰方酣，奧斯若恩與阿迪亞貝尼起兵反羅馬，圍攻尼西比斯。"
F.append(o)
h = copy.deepcopy(by["哈特拉"]); h["intro"] = "沙漠城邦哈特拉之王阿卜德薩米亞，193年遣弓騎兵援助尼格爾。"; F.append(h)
ps = copy.deepcopy(by["波斯"])
ps["intro"] = "法爾斯（波西斯）巴茲蘭吉王朝的戈契赫爾，帕提亞的藩王，都伊斯塔赫爾。祭司薩珊之子帕佩克在其治下。"
ps["officers"] = [x for x in ps["officers"] if x["p"] != "Ardashir I"]          # Ardashir born c. 180: a boy in 194
F.append(ps)
c = copy.deepcopy(by["查拉塞涅"]); c["ruler"] = "Attambelos VIII"; c["officers"] = [O("Attambelos VIII", "查拉")]
c["intro"] = "波斯灣頭的查拉塞涅（梅珊）王國，帕提亞的藩屬。國王阿坦貝洛斯八世約180年至195年在位，主要見於錢幣。"; F.append(c)
F.append(copy.deepcopy(by["埃利邁斯"]))
F.append(copy.deepcopy(by["貴霜"]))
w = copy.deepcopy(by["西方總督"]); w["ruler"] = "Rudrasimha I"
w["officers"] = [O("Rudrasimha I", "優禪"), O("Jivadaman", "優禪", birth=168), O("Satyadaman", "婆盧")]
w["intro"] = "統治印度西部的塞種卡爾達馬卡王朝（西方總督）。魯陀羅辛哈一世曾於188年被阿毗羅人伊濕伐羅達多所逐，190年復位。"
F.append(w)
for n in ("百乘", "師子國", "潘地亞", "那伽", "穆倫達", "圖蘭", "薩卡斯坦", "博斯普魯斯", "伊比利亞", "喀里多尼亞", "夸迪",
          "阿克蘇姆", "庫施", "示巴", "希木葉爾", "哈德拉毛", "于闐", "車師"):
    F.append(copy.deepcopy(by[n]))
for f in F:
    if f["name"] in ("阿克蘇姆", "希木葉爾"):           # the princes BYGT and Dhamar'ali Yuhabirr are boys in 194
        f["officers"] = f["officers"][:1]
    f["intro"] = f["intro"].replace("198年", "194年")
dump({"base": 2, "note": "194年（群雄割據）的世界：羅馬內戰（塞維魯、尼格爾、阿爾比努斯）、安息及其藩國、貴霜、印度諸國、阿拉伯、東非、黑海、高加索、鮮卑諸部、西域。",
      "forces": F}, sys.argv[1])
