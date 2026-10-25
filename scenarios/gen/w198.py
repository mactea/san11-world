import sys
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import *

F = []
F.append({"name": "羅馬", "ruler": "Septimius Severus", "strategist": "Gaius Fulvius Plautianus", "stars": 1,
  "intro": "塞普提米烏斯·塞維魯平定內戰後東征帕提亞，197年末攻陷泰西封，198年立長子卡拉卡拉為奧古斯都、次子蓋塔為凱撒。本劇本中羅馬只保有意大利、西西里、撒丁、科西嘉、達爾馬提亞、諾里庫姆、納博高盧與阿非利加等核心之地，其餘行省無主。",
  "cities": ROME_CITIES,
  "officers": [O("Septimius Severus", "羅馬"), O("Caracalla", "羅馬"), O("Geta (emperor)", "羅馬"), O("Julia Domna", "羅馬"),
               O("Gaius Fulvius Plautianus", "羅馬"), O("Castor (freedman of Severus)", "羅馬", birth=160),
               # generals and governors: the core city nearest to where they were in 198
               O("Probus (general of Severus)", near="尼西"), O("Valerianus (general of Severus)", near="尼西"),
               O("Subatianus Aquila", near="尼西"), O("Lucius Alfenus Senecio", near="安都"),
               O("Cassius Dio", "羅馬"), O("Aemilius Papinianus", "羅馬"), O("Julia Maesa", "羅馬"), O("Julius Avitus", "羅馬"),
               O("Publius Cornelius Anullinus", "羅馬"), O("Lucius Fabius Cilo", near="卡農"), O("Tiberius Claudius Candidus", near="塔拉"),
               O("Quintus Caecilius Pudens", near="奧格"), O("Quintus Aemilius Saturninus", near="黎軒"), O("Macrinus", near="約爾")],
  "auto": dict(ROME_AUTO, from_cities=ROME_PROV),
  "relations": {"安息": 10, "哈特拉": 5, "奧斯若恩": 85, "博斯普魯斯": 85, "亞美尼亞": 65, "伊比利亞": 60, "喀里多尼亞": 20,
                "夸迪": 55, "波斯": 40, "查拉塞涅": 40, "阿克蘇姆": 60, "庫施": 50, "貴霜": 60}})
F.append({"name": "安息", "ruler": "Vologases V", "stars": 3,
  "intro": "帕提亞國王沃洛吉斯五世。內戰中支持尼格爾，197年冬塞維魯沿幼發拉底河南下攻破泰西封，大肆劫掠，安息國勢大衰。諸子沃洛吉斯、阿爾達班據米底等地，諸藩國漸生異心。",
  "cities": [{"city": "斯賓", "gold": 2000}, "阿蠻", "拉蓋", "赫卡", "阿貝"],
  "officers": [O("Vologases V", "斯賓"), O("Vologases VI", "斯賓", birth=175), O("Artabanus V of Parthia", "阿蠻")],
  "relations": {"波斯": 70, "查拉塞涅": 75, "埃利邁斯": 75, "哈特拉": 70, "亞美尼亞": 60, "奧斯若恩": 30, "圖蘭": 60,
                "薩卡斯坦": 65, "貴霜": 40, "伊比利亞": 50}})
F.append({"name": "亞美尼亞", "kokugo_name": "亞美", "ruler": "Khosrov I of Armenia", "stars": 4,
  "intro": "亞美尼亞阿薩息斯王朝國王庫思老一世，周旋於羅馬與帕提亞之間。塞維魯東征時遣使送人質與禮物，保住王位。",
  "cities": ["亞塔"],
  "officers": [O("Khosrov I of Armenia", "亞塔", birth=165), O("Tiridates (satrap)", "亞塔")],
  "relations": {"伊比利亞": 80}})
F.append({"name": "奧斯若恩", "kokugo_name": "奧斯", "ruler": "Abgar VIII", "stars": 5,
  "intro": "埃德薩的阿布加爾八世。195年後國土大半併入羅馬新設的奧斯若恩行省，僅保有都城埃德薩，為羅馬附庸。宮中有學者巴爾德桑。",
  "cities": ["埃德"],
  "officers": [O("Abgar VIII", "埃德", birth=167), O("Bardaisan", "埃德")],
  "relations": {}})
F.append({"name": "哈特拉", "kokugo_name": "哈特", "ruler": "Abdsamiya", "stars": 5,
  "intro": "沙漠城邦哈特拉之王阿卜德薩米亞。曾遣弓騎兵助尼格爾，198年塞維魯親率大軍圍城，城中弓手擲毒蟲陶罐拒守，羅馬軍無功而退。",
  "cities": ["哈特"],
  "officers": [O("Abdsamiya", "哈特"), O("Sanatruq II", "哈特", birth=175)],
  "relations": {}})
F.append({"name": "波斯", "ruler": "Gochihr", "stars": 5,
  "intro": "法爾斯（波西斯）巴茲蘭吉王朝的戈契赫爾，帕提亞的藩王，都伊斯塔赫爾。祭司薩珊之子帕佩克與年少的阿爾達希爾在其治下。",
  "cities": ["波斯"],
  "officers": [O("Gochihr", "波斯", birth=155), O("Papak", "波斯"), O("Sasan", "波斯"), O("Tiri (argbed of Darabgerd)", "波斯"),
               O("Ardashir I", "波斯"), O("Pasin of Gopanan", "波斯", birth=165), O("Dara of Lurwir", "波斯", birth=165),
               O("Manuchihr of Konus", "波斯")],
  "relations": {"查拉塞涅": 50, "埃利邁斯": 55}})
F.append({"name": "查拉塞涅", "kokugo_name": "梅珊", "ruler": "Maga of Characene", "stars": 5,
  "intro": "波斯灣頭的查拉塞涅（梅珊）王國，帕提亞的藩屬，控制兩河入海口的貿易。國王馬加事蹟所傳甚少。",
  "cities": ["查拉"],
  "officers": [O("Maga of Characene", "查拉")],
  "relations": {}})
F.append({"name": "埃利邁斯", "kokugo_name": "埃利", "ruler": "Orodes V.", "stars": 5,
  "intro": "札格羅斯山中的埃利邁斯王國，帕提亞阿薩息斯王族的旁支，國王奧羅德五世。蘇薩的總督赫瓦薩克亦見於碑銘。",
  "cities": ["蘇薩"],
  "officers": [O("Orodes V.", "蘇薩"), O("Khwasak", "蘇薩")],
  "relations": {}})
F.append({"name": "貴霜", "ruler": "Vasudeva I", "stars": 3,
  "intro": "貴霜大王韋蘇提婆一世，繼胡維色迦即位，仍統有大夏、犍陀羅至秣菟羅的廣大疆土，是最後一位統治帝國全盛疆域的貴霜君主。",
  "cities": ["富樓", "秣菟", "藍氏", "罽賓", "高附"],
  "officers": [O("Vasudeva I", "富樓", birth=165)],
  "relations": {"西方總督": 50, "那伽": 40, "于闐": 60}})
F.append({"name": "西方總督", "kokugo_name": "塞種", "ruler": "Jivadaman", "stars": 4,
  "intro": "統治印度西部的塞種卡爾達馬卡王朝（西方總督）。魯陀羅辛哈一世之後，耆婆達曼於197年復位，與薩提亞達曼爭立；王子魯陀羅犀那不久將繼位。",
  "cities": ["優禪", "婆盧", "蘇剌", "喀奇", "摩臘"],
  "officers": [O("Jivadaman", "優禪", birth=168), O("Satyadaman", "婆盧"), O("Rudrasena I (Saka king)", "優禪")],
  "relations": {"百乘": 15, "那伽": 40}})
F.append({"name": "百乘", "ruler": "Yajna Sri Satakarni", "stars": 3,
  "intro": "德干的百乘（娑多婆訶那）王朝，雅吉納·室利·薩塔卡尼在位，擊敗西方總督、收復西部失地，是王朝最後的強主。大乘論師龍樹與其交好。",
  "cities": ["缽底", "案達", "那私", "毗陀", "果提"],
  "officers": [O("Yajna Sri Satakarni", "缽底", birth=145), O("Gautamiputra Vijaya Satakarni", "案達"), O("Nagarjuna", "案達")],
  "relations": {"那伽": 50}})
F.append({"name": "師子國", "kokugo_name": "師子", "ruler": "Siri Naga I", "stars": 5,
  "intro": "錫蘭島的阿努拉德普勒王國，斯里那伽一世於189年弒庫達那伽即位，修繕大塔，重建五層銅殿。",
  "cities": ["師子", "盧呼"],
  "officers": [O("Siri Naga I", "師子")],
  "relations": {"潘地亞": 40}})
F.append({"name": "潘地亞", "kokugo_name": "潘地", "ruler": "Nedunjeliyan II", "stars": 5,
  "intro": "泰米爾地區的潘地亞王國，都馬杜賴。桑伽姆詩歌頌揚內頓傑利延於塔萊亞拉加南大破朱羅、哲羅聯軍（其年代學者說法不一）。",
  "cities": ["摩頭"],
  "officers": [O("Nedunjeliyan II", "摩頭", birth=175)],
  "relations": {}})
F.append({"name": "那伽", "ruler": "Vrisha-naga", "stars": 5,
  "intro": "毗底沙一帶的那伽王族，錢幣上見弗利沙那伽之名。貴霜南方勢力衰退後，那伽諸王將崛起於印度中北部。",
  "cities": ["毗舍"],
  "officers": [O("Vrisha-naga", "毗舍")],
  "relations": {}})
F.append({"name": "穆倫達", "kokugo_name": "穆倫", "ruler": "Gana (Murunda)", "stars": 5,
  "intro": "東印度羯陵伽、烏特卡拉一帶的穆倫達王朝，摩訶羅闍伽那見於跋陀羅迦銘文（年代約在二世紀）。",
  "cities": ["羯陵"],
  "officers": [O("Gana (Murunda)", "羯陵")],
  "relations": {}})
F.append({"name": "圖蘭", "ruler": "Pahares I", "stars": 5,
  "intro": "印度—帕提亞殘餘之一的圖蘭王國，帕哈雷斯一世在位，錢幣王像戴帕提亞式頭冠。",
  "cities": ["奎達"],
  "officers": [O("Pahares I", "奎達", birth=160)],
  "relations": {"薩卡斯坦": 70}})
F.append({"name": "薩卡斯坦", "kokugo_name": "薩卡", "ruler": "Farn-Sasan", "stars": 5,
  "intro": "錫斯坦的印度—帕提亞王國（薩卡斯坦）。198年的國王不詳，此處取年代最近的末代國王法恩·薩珊（約210年起在位，只見於錢幣）。",
  "cities": ["錫斯"],
  "officers": [O("Farn-Sasan", "錫斯", birth=175)],
  "relations": {}})
F.append({"name": "博斯普魯斯", "kokugo_name": "博斯", "ruler": "Tiberius Julius Sauromates II", "stars": 5,
  "intro": "黑海北岸的博斯普魯斯王國，羅馬的附庸。薩烏羅馬特斯二世174年即位，征討西徐亞、陶里諸族，保障黑海商路。",
  "cities": ["潘提", "塔奈", "庫班"],
  "officers": [O("Tiberius Julius Sauromates II", "潘提", birth=155)],
  "relations": {}})
F.append({"name": "伊比利亞", "kokugo_name": "伊比", "ruler": "Rev I of Iberia", "stars": 5,
  "intro": "高加索的伊比利亞（卡特利）王國。亞美尼亞王子雷夫一世189年應貴族之請攻殺舅父阿馬扎斯普二世即位，開阿薩息斯一系。",
  "cities": ["姆茨"],
  "officers": [O("Rev I of Iberia", "姆茨", birth=165), O("Sephelia", "姆茨")],
  "relations": {}})
F.append({"name": "喀里多尼亞", "kokugo_name": "喀里", "ruler": "Argentocoxos", "stars": 5,
  "intro": "哈德良長城以北的喀里多尼亞諸部。197年不列顛總督以重金向邁亞泰人買和。198年的首領不詳，此處取狄奧所記約210年的首領阿根托科索斯。",
  "cities": ["喀里", "皮克"],
  "officers": [O("Argentocoxos", "喀里", birth=165)],
  "relations": {}})
F.append({"name": "夸迪", "ruler": "Gabiomaro", "stars": 5,
  "intro": "多瑙河北岸的日耳曼夸迪人，馬科曼尼戰爭後為羅馬的附庸。198年的國王不詳，此處取狄奧所記、後被卡拉卡拉處死的蓋奧博馬魯斯。",
  "cities": ["夸迪"],
  "officers": [O("Gabiomaro", "夸迪", birth=170)],
  "relations": {}})
F.append({"name": "阿克蘇姆", "kokugo_name": "阿克", "ruler": "GDRT", "stars": 4,
  "intro": "紅海西岸的阿克蘇姆王國，國王葛達拉（GDRT）約200年與示巴王阿爾漢結盟，出兵渡海干預南阿拉伯。",
  "cities": ["阿克", "貢德"],
  "officers": [O("GDRT", "阿克"), O("BYGT", "阿克")],
  "relations": {"示巴": 80, "希木葉爾": 30}})
F.append({"name": "庫施", "ruler": "Amanitaraqide", "stars": 5,
  "intro": "尼羅河上游的庫施王國，都麥羅埃。二世紀末的君主次序不明，此處取約二世紀末在位的阿瑪尼塔拉基德。",
  "cities": ["麥羅", "納帕", "東古"],
  "officers": [O("Amanitaraqide", "麥羅")],
  "relations": {}})
F.append({"name": "示巴", "ruler": "’Alhan Nahfan", "stars": 4,
  "intro": "南阿拉伯的示巴王國，都馬里布。國王阿爾漢·納赫凡與其子沙伊爾·奧塔爾共治，聯合阿克蘇姆對抗希木葉爾。",
  "cities": ["馬里"],
  "officers": [O("’Alhan Nahfan", "馬里"), O("Sha'ir Awtar", "馬里", birth=170)],
  "relations": {"希木葉爾": 15, "哈德拉毛": 60}})
F.append({"name": "希木葉爾", "kokugo_name": "希木", "ruler": "Tharan Yaub Yuhanim", "stars": 4,
  "intro": "南阿拉伯高原的希木葉爾王國，都札法爾，國王自稱「薩巴與賴丹王」，與示巴爭奪葉門。",
  "cities": ["希木"],
  "officers": [O("Tharan Yaub Yuhanim", "希木", birth=150), O("Dhamar'ali Yuhabirr", "希木")],
  "relations": {"哈德拉毛": 40}})
F.append({"name": "哈德拉毛", "kokugo_name": "哈德", "ruler": "Il'azz Yalut", "stars": 5,
  "intro": "南阿拉伯東部的哈德拉毛王國，都沙布瓦，控制乳香產地。198年的國王不詳，此處取年代最近的伊勒阿茲·亞盧特。",
  "cities": ["沙布", "祖法"],
  "officers": [O("Il'azz Yalut", "沙布", birth=175)],
  "relations": {}})
F.append({"name": "于闐", "ruler": "山習", "stars": 5,
  "intro": "塔里木盆地南緣的于闐國。198年的國王不詳，此處取魏文帝時遣使獻馬的于闐王山習。",
  "cities": ["于闐"],
  "officers": [O("山習", "于闐", birth=170)],
  "relations": {"車師": 50}})
F.append({"name": "車師", "ruler": "壹多雜", "stars": 5,
  "intro": "天山東部的車師後部。198年的國王不詳，此處取魏時受「守魏侍中」之號的車師後部王壹多雜。",
  "cities": ["車師"],
  "officers": [O("壹多雜", "車師", birth=170)],
  "relations": {}})
# Xianbei factions after Tanshihuai: regular forces named after the leader (no 国号, like Koei's warlords)
F.append({"name": "步度根", "kokugo": False, "ruler": "步度根", "stars": 4,
  "intro": "檀石槐死後鮮卑分裂。魁頭死，弟步度根繼立，統檀石槐舊部，然眾稍衰弱，其兄扶羅韓別擁眾數萬為大人。",
  "cities": ["龍城", "燕然"],
  "officers": [O("步度根", "龍城", born=167), O("扶羅韓", "龍城", born=165)],
  "relations": {"軻比能": 40, "素利": 50, "曹操": 30, "袁紹": 50, "公孫瓚": 30}})
F.append({"name": "軻比能", "kokugo": False, "ruler": "軻比能", "stars": 4,
  "intro": "小種鮮卑軻比能，以勇健、斷法平端被推為大人。袁紹據河北，中國人多亡歸之，教作兵器鎧楯，頗學文字。",
  "cities": ["狼居"],
  "officers": [O("軻比能", "狼居"), O("苴羅侯", "狼居", birth=175)],
  "relations": {"素利": 30, "曹操": 30, "袁紹": 50, "公孫瓚": 20}})
F.append({"name": "素利", "kokugo": False, "ruler": "素利", "stars": 4,
  "intro": "東部鮮卑大人素利、彌加、厥機，在遼西、右北平、漁陽塞外，種眾多於軻比能。建安中因閻柔通貢。",
  "cities": ["呼倫", "室韋"],
  "officers": [O("素利", "呼倫"), O("彌加", "呼倫"), O("厥機", "室韋", born=160), O("成律歸", "呼倫")],
  "relations": {"曹操": 40, "袁紹": 60, "公孫瓚": 10, "公孫度": 40}})

xb = F[-3:]
F = F[:2] + xb + F[2:-3]
dump({"base": 8, **RESOURCES, "note": "198年（呂布討伐戰）的世界：羅馬（塞維魯東征）、安息及其藩國、貴霜、印度諸國、阿拉伯、東非、黑海、高加索、鮮卑諸部、西域。剧本说明见 docs/剧本制作.md。",
      "forces": F}, sys.argv[1])
