"""Rank local ground materials using existing map data, without game execution.

Hex association uses the same nearest-hex mapping as make_world.learn_materials.
These are static vertex counts, not visible-pixel counts or runtime hit rates.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
from inventory_terrain_assets import ROOT, ground_contract

NAMES = ['草地','土','砂','湿地','毒泉','森林','溪流','河','海','荒地','道路','栈道','桥','浅滩','岸','山','城','关','港','小径']


def analyze():
    shex_path=ROOT.parent/'data/china_shex.npy'
    vertex_path=ROOT.parent/'data/china_k3st_v.npy'
    shex=np.load(shex_path,mmap_mode='r',allow_pickle=False)
    vertices=np.load(vertex_path,mmap_mode='r',allow_pickle=False)
    if shex.shape!=(200,200,11) or vertices.shape!=(1025,1025,8) or shex.dtype!=np.uint8 or vertices.dtype!=np.uint8:
        raise ValueError('unexpected local China map layout')
    z,x=np.mgrid[112:912,112:912]
    lo=np.clip(np.round((z-114)/4).astype(int),0,199)
    hi=np.clip(np.round((x-114-2*(lo&1))/4).astype(int),0,199)
    material=vertices[112:912,112:912,7]
    terrain=shex[lo,hi,0]
    rows=[]
    for index in range(36):
        selected=material==index
        count=int(selected.sum())
        frequencies=np.bincount(terrain[selected],minlength=256)
        order=np.argsort(-frequencies)
        rows.append({'material_index':index,'china_vertices':count,'china_share_percent':count/640000*100,
                     'hex_association':[{'type':int(t),'name':NAMES[t] if t<len(NAMES) else str(t),
                                         'vertices':int(frequencies[t]),'percent':float(frequencies[t]/count*100)}
                                        for t in order if frequencies[t] and count]})
    world_path=ROOT.parent/'data/world/world_mat.npy'
    world_info=None
    if world_path.is_file():
        world=np.load(world_path,mmap_mode='r',allow_pickle=False)
        if world.ndim!=2 or world.dtype!=np.uint8:raise ValueError('unexpected world material array')
        counts=np.zeros(256,dtype=np.int64)
        for start in range(0,world.shape[0],128):counts+=np.bincount(world[start:start+128].ravel(),minlength=256)
        for row in rows:row['world_vertices']=int(counts[row['material_index']]);row['world_share_percent']=row['world_vertices']/world.size*100
        world_info={'path':str(world_path),'shape':list(world.shape),'sha256':hashlib.sha256(world_path.read_bytes()).hexdigest(),
                    'outside_ground_table_vertices':int(counts[36:].sum())}
    return {'game_launched':False,'contract':ground_contract(ROOT/'build/render-game/san11pk_render.exe'),
            'china_inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (shex_path,vertex_path)},
            'world':world_info,'mapping':'nearest hex; same coordinate formula as tools/make_world.py learn_materials',
            'limitations':['statistical association, not a one-to-one terrain label','vertex count is not screen coverage','world file is build data, not a live save snapshot'],
            'materials':sorted(rows,key=lambda row:row.get('world_vertices',row['china_vertices']),reverse=True)}


if __name__=='__main__':
    result=analyze();output=ROOT/'build/terrain-assets/material-usage.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({'output':str(output),'top_materials':[{'index':r['material_index'],'world_share':r.get('world_share_percent'),
                      'main_hex_types':[x['name'] for x in r['hex_association'][:2]]} for r in result['materials'][:6]]}))
