"""Build the original merchant-ship mesh, OBJ/GLB and a native WKMD candidate.

Y is up; Z is the keel. No game files are written. The native export borrows only
the owner's boat rig/metadata from resource 0x8ad, not its hull vertices/texture.
"""
import argparse
import json
import math
import struct
from pathlib import Path

import numpy as np

VISUAL_SCALE = 1.5  # Enlarge the merchant mesh about its waterline; keep the native rig unchanged.

COLORS = {
    'hull': (79, 46, 29), 'strake': (125, 75, 40), 'deck': (173, 124, 70),
    'beam': (77, 44, 29), 'trim': (143, 103, 60), 'iron': (55, 59, 57),
    'sail': (224, 217, 191), 'seam': (171, 146, 102), 'rope': (131, 111, 74),
    'crate': (163, 103, 53), 'cover': (105, 119, 101), 'flag': (49, 86, 98),
    'roof': (128, 111, 79), 'dark': (45, 36, 27), 'pottery': (148, 77, 46),
}


class Mesh:
    def __init__(self):
        self.parts = {}
        self.part = ''

    def triangle(self, a, b, c, material):
        a, b, c = (np.asarray(v, dtype=float) for v in (a, b, c))
        normal = np.cross(b-a, c-a)
        length = np.linalg.norm(normal)
        if length < 1e-9:
            raise ValueError(f'degenerate triangle in {self.part}')
        self.parts.setdefault(self.part, []).append((np.array([a,b,c]), normal/length, material))

    def quad(self, a,b,c,d,mat):
        self.triangle(a,b,c,mat); self.triangle(a,c,d,mat)

    def box(self, center, size, mat):
        x,y,z=center; dx,dy,dz=np.array(size)/2
        v=[(x+a*dx,y+b*dy,z+c*dz) for a,b,c in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for f in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)]:
            self.quad(*(v[i] for i in f),mat)

    def rod(self,a,b,r,mat,sides=6):
        a,b=np.array(a,float),np.array(b,float); axis=b-a;axis/=np.linalg.norm(axis)
        right=np.cross(axis,[0,1,0] if abs(axis[1])<.9 else [1,0,0]);right/=np.linalg.norm(right)
        up=np.cross(axis,right)
        ring=[r*(math.cos(i*2*math.pi/sides)*right+math.sin(i*2*math.pi/sides)*up) for i in range(sides)]
        for i,v in enumerate(ring):
            w=ring[(i+1)%sides]
            self.quad(a+v,a+w,b+w,b+v,mat)
            self.triangle(a,a+w,a+v,mat);self.triangle(b,b+v,b+w,mat)

    def triangles(self):
        return [t for part in self.parts.values() for t in part]


def build_mesh():
    m=Mesh()
    stations=[(-6.2,.10,.65),(-5.5,.85,.35),(-4.4,1.75,.10),(-2.5,2.32,0),
              (0,2.48,0),(2.6,2.35,.12),(4.4,2.0,.50),(5.5,1.70,.70)]
    m.part='01_deep_ocean_hull'
    rings=[]
    for z,w,r in stations:
        rings.append([(-w,2.15+r,z),(-w*.98,1.38+r,z),(-w*.84,.35+r,z),(-w*.46,-.65+r,z),
                      (0,-1.15+r,z),(w*.46,-.65+r,z),(w*.84,.35+r,z),(w*.98,1.38+r,z),(w,2.15+r,z)])
    for a,b in zip(rings,rings[1:]):
        for j in range(8):m.quad(a[j+1],b[j+1],b[j],a[j],'strake' if j in [0,7] else 'hull')
        m.quad(b[0],b[-1],a[-1],a[0],'deck')
    for ring,reverse in [(rings[0],False),(rings[-1],True)]:
        mid=np.mean(ring,axis=0)
        for j in range(len(ring)):
            a,b=ring[j],ring[(j+1)%len(ring)]
            m.triangle(mid,a,b,'hull') if reverse else m.triangle(mid,b,a,'hull')

    def rail(a,b):
        a,b=np.array(a,float),np.array(b,float)
        for t in np.linspace(0,1,max(2,int(np.linalg.norm(b-a)/.48)+1)):
            p=a+(b-a)*t;m.rod(p,p+[0,.42,0],.033,'trim',4)
        m.rod(a+[0,.42,0],b+[0,.42,0],.055,'trim',4)
        m.rod(a+[0,.12,0],b+[0,.12,0],.025,'beam',4)

    m.part='02_hull_wales_and_bulwarks'
    for side in [-1,1]:
        for (z,w,r),(nz,nw,nr) in zip(stations,stations[1:]):
            for y,scale,rad,mat in [(2.23,1,.085,'beam'),(1.38,.98,.085,'beam'),(.35,.84,.065,'beam'),(1.88,.997,.035,'trim')]:
                m.rod((side*w*scale,y+r,z),(side*nw*scale,y+nr,nz),rad,mat,6)
            if z>=-4.4 and nz<=2.6:rail((side*w,2.25+r,z),(side*nw,2.25+nr,nz))
    m.part='03_deck_planking'
    for (z,w,r),(nz,nw,nr) in zip(stations,stations[1:]):
        for t in np.linspace(0,1,max(2,int((nz-z)/.4)),endpoint=False):
            ww=w+(nw-w)*t;yy=2.157+r+(nr-r)*t;zz=z+(nz-z)*t
            m.rod((-ww+.04,yy,zz),(ww-.04,yy,zz),.009,'beam',4)

    def canopy(name,z,length,half,eave,rise):
        m.part=name
        arc=[(half*math.cos(t),eave+rise*math.sin(t)) for t in np.linspace(math.pi,0,9)]
        for (x,y),(nx,ny) in zip(arc,arc[1:]):
            a,b,c,d=(x,y,z-length/2),(x,y,z+length/2),(nx,ny,z+length/2),(nx,ny,z-length/2)
            m.quad(a,b,c,d,'roof');m.quad(d,c,b,a,'roof')
            for zz in [z-length/2,z,z+length/2]:m.rod((x,y+.02,zz),(nx,ny+.02,zz),.025,'beam',4)
        for x in [-half,half]:
            for zz in [z-length/2,z+length/2]:m.rod((x,2.36,zz),(x,eave,zz),.045,'beam',6)
            m.rod((x,eave,z-length/2),(x,eave,z+length/2),.045,'beam',6)

    # Layered shelters draw on Han boat models; their sea-going proportions are artistic inference.
    canopy('04_low_bow_shelter',-4.85,1.10,.85,2.75,.42)
    m.part='05_stern_steering_cabin'
    m.box((0,3.10,4.25),(2.55,1.15,2.25),'strake')
    m.box((0,2.55,4.25),(2.78,.14,2.46),'deck')
    for side in [-1,1]:
        for z in [3.14,4.25,5.36]:m.box((side*1.29,3.10,z),(.075,1.18,.075),'beam')
        m.box((side*1.29,3.22,4.25),(.035,.43,.64),'dark')
        m.box((side*1.32,3.22,4.25),(.035,.46,.045),'beam')
        m.box((side*1.29,2.66,4.25),(.065,.075,2.32),'beam')
        rail((side*1.68,2.87,3.3),(side*1.68,2.87,5.45))
    m.part='06_plain_pitched_cabin_roof'
    roof=[(-1.40,3.70),(0,4.06),(1.40,3.70)]
    for (x,y),(nx,ny) in zip(roof,roof[1:]):
        a,b,c,d=(x,y,3.0),(x,y,5.51),(nx,ny,5.51),(nx,ny,3.0)
        m.quad(a,b,c,d,'roof');m.quad(d,c,b,a,'roof')
        for zz in [3.0,3.63,4.26,4.89,5.51]:m.rod((x,y+.02,zz),(nx,ny+.02,zz),.025,'beam',4)
    for z in [3.12,5.38]:
        a,b,c=(-1.27,3.67,z),(0,4.0,z),(1.27,3.67,z)
        m.triangle(a,b,c,'strake');m.triangle(c,b,a,'strake')
    m.rod((0,4.09,2.98),(0,4.09,5.53),.045,'beam',6)
    m.part='07_cabin_entrance'
    m.box((0,3.0,3.108),(.62,.92,.035),'dark')
    for x in [-.35,.35]:m.box((x,3.0,3.08),(.06,1.0,.05),'beam')
    for i in range(2):m.box((0,2.35+i*.14,2.75+i*.17),(.68,.12,.26),'deck')

    m.part='08_large_cargo_hatches'
    for z,length in [(.95,1.6),(-2.15,1.15)]:
        m.box((0,2.36,z),(2.22,.38,length),'beam')
        m.box((0,2.57,z),(2.12,.09,length-.08),'cover')
        for x in [-.7,0,.7]:m.rod((x,2.63,z-length*.46),(x,2.63,z+length*.46),.025,'rope',4)
        for zz in [z-length*.42,z+length*.42]:m.rod((-1.05,2.64,zz),(1.05,2.64,zz),.025,'rope',4)
    canopy('08_mid_cargo_shelter',1.05,1.85,1.20,2.85,.55)
    m.part='09_secured_deck_cargo'
    for x,z,s in [(-1.55,-1.25,.62),(1.55,-1.25,.62),(-1.58,1.0,.56),(1.58,1.0,.56)]:
        m.box((x,2.51,z),(s,.65,s),'crate')
        for yy in [2.24,2.78]:m.box((x,yy,z),(s+.03,.06,s+.03),'beam')
        for dx in [-s*.3,s*.3]:m.box((x+dx,2.51,z),(.045,.68,s+.04),'trim')
    m.part='10_anchor_and_oar_supports'
    for side in [-1,1]:
        for z,x in [(-3.9,1.94),(-3.3,2.12)]:
            m.rod((side*x,2.33,z),(side*x,2.85,z),.05,'beam',6)
            m.rod((side*(x-.18),2.85,z),(side*(x+.18),2.85,z),.05,'beam',6)
    m.box((.55,2.68,-4.15),(.42,.25,.65),'iron')
    for x in [.35,.75]:m.rod((x,2.62,-4.75),(x,2.65,-3.63),.06,'beam',6)
    m.rod((.55,2.82,-4.1),(.2,2.8,-5.6),.028,'rope',4)

    def square_sail(name,z,bottom,top,half):
        m.part=name+'_canvas'
        def surface(u,v):
            return ((2*u-1)*half*(1-.15*v),bottom+(top-bottom)*v,
                    z+.13+.48*math.sin(math.pi*u)*math.sin(math.pi*v))
        for i in range(8):
            for j in range(6):
                a,b,c,d=surface(i/8,j/6),surface((i+1)/8,j/6),surface((i+1)/8,(j+1)/6),surface(i/8,(j+1)/6)
                m.quad(a,b,c,d,'sail');m.quad(d,c,b,a,'sail')
        m.part=name+'_seams_and_yards'
        for u in [.25,.5,.75]:
            for j in range(6):m.rod(surface(u,j/6),surface(u,(j+1)/6),.009,'seam',4)
        for v in [0,1]:
            for i in range(8):m.rod(surface(i/8,v),surface((i+1)/8,v),.023,'rope',4)
        for v in [0,1]:
            y=bottom+(top-bottom)*v;w=half*(1-.15*v)+.13
            m.rod((-w,y,z+.1),(w,y,z+.1),.055 if v else .035,'beam',6)
        for u in [0,1]:
            for j in range(6):m.rod(surface(u,j/6),surface(u,(j+1)/6),.025,'rope',4)

    # Two single-tier sails are a restrained game reconstruction, not a documented standard rig.
    for name,z,top,half,low in [('main',-.25,9.1,2.65,3.65),('fore',-3.5,7.15,1.95,3.4)]:
        m.part='11_'+name+'_mast'
        m.rod((0,2.15,z),(0,top,z),.105 if name=='main' else .09,'beam',8)
        m.box((0,2.37,z),(.48,.42,.48),'beam')
        square_sail('12_'+name+'_sail',z,low,top-.5,half)
        m.part='14_'+name+'_shrouds'
        for side in [-1,1]:
            for dz in [.50,1.20]:m.rod((0,top-.35,z),(side*2.12,2.4,z+dz),.022,'rope',4)
        m.rod((0,top-.12,z),(0,2.83,-5.8 if name=='fore' else 2.6),.022,'rope',4)
    m.part='16_trade_pennants'
    for z,y,width in [(-.25,9.05,.80)]:
        a,b,c,d=(0,y-.32,z),(0,y+.24,z),(width,y+.13,z+.18),(width*.72,y-.2,z+.18)
        m.quad(a,b,c,d,'flag');m.quad(d,c,b,a,'flag')
    m.part='17_rudder_and_mooring'
    m.rod((0,-.8,5.73),(0,3.0,5.73),.095,'beam',8)
    m.box((0,.05,5.96),(.18,2.0,.8),'hull')
    m.rod((0,2.95,5.73),(0,2.95,5.35),.07,'beam',6)
    for z,y in [(-5.65,2.78),(2.25,2.45)]:
        for x in [-.55,.55]:m.rod((x,y,z),(x,y+.30,z),.08,'beam',6)
    for triangles in m.parts.values():
        for points,normal,material in triangles:points *= VISUAL_SCALE
    return m

def export_obj(m,out):
    rows=['# Original merchant ship; metres-like game units; Y up','mtllib merchant_ship.mtl']
    count=0
    for part,ts in m.parts.items():
        rows.append('o '+part);last=None
        for points,n,mat in ts:
            if mat!=last:rows.append('usemtl '+mat);last=mat
            for v in points:rows.append('v '+' '.join(f'{x:.6f}' for x in v))
            rows.append('vn '+' '.join(f'{x:.6f}' for x in n))
            normal=count//3+1
            rows.append('f '+' '.join(f'{count+j+1}//{normal}' for j in range(3)));count+=3
    (out/'merchant_ship.obj').write_text('\n'.join(rows)+'\n',encoding='ascii')
    (out/'merchant_ship.mtl').write_text('\n'.join(f'newmtl {k}\nKd '+ ' '.join(f'{x/255:.5f}' for x in c)+'\nKa 0.15 0.15 0.15\nKs 0 0 0\nNs 8\nd 1\n' for k,c in COLORS.items()),encoding='ascii')


def export_glb(m,out):
    blob=bytearray();views=[];accessors=[];meshes=[];nodes=[]
    mats=list(COLORS)
    def buffer(values,kind,typ):
        while len(blob)%4:blob.append(0)
        at=len(blob);arr=np.array(values,dtype='<f4');blob.extend(arr.tobytes())
        views.append(dict(buffer=0,byteOffset=at,byteLength=arr.nbytes,target=34962))
        a=dict(bufferView=len(views)-1,componentType=5126,count=len(arr),type=typ)
        if kind=='POSITION':a.update(min=arr.min(axis=0).tolist(),max=arr.max(axis=0).tolist())
        accessors.append(a);return len(accessors)-1
    for name,triangles in m.parts.items():
        prims=[]
        for mat in mats:
            ts=[t for t in triangles if t[2]==mat]
            if not ts:continue
            v=[v for p,n,_ in ts for v in p];norm=[n for p,n,_ in ts for _ in p]
            prims.append(dict(attributes=dict(POSITION=buffer(v,'POSITION','VEC3'),NORMAL=buffer(norm,'NORMAL','VEC3')),mode=4,material=mats.index(mat)))
        meshes.append(dict(name=name,primitives=prims));nodes.append(dict(name=name,mesh=len(meshes)-1))
    doc=dict(asset=dict(version='2.0',generator='san11world-trade original merchant ship'),scene=0,scenes=[dict(nodes=list(range(len(nodes))))],nodes=nodes,meshes=meshes,
             materials=[dict(name=k,pbrMetallicRoughness=dict(baseColorFactor=[*(v/255 for v in COLORS[k]),1],metallicFactor=0,roughnessFactor=.9),doubleSided=False) for k in mats],
             buffers=[dict(byteLength=len(blob))],bufferViews=views,accessors=accessors)
    text=json.dumps(doc,separators=(',',':')).encode();text+=b' '*((-len(text))%4);blob+=b'\0'*((-len(blob))%4)
    data=struct.pack('<4sII',b'glTF',2,12+8+len(text)+8+len(blob))+struct.pack('<I4s',len(text),b'JSON')+text+struct.pack('<I4s',len(blob),b'BIN\0')+blob
    (out/'merchant_ship.glb').write_bytes(data)


def resource(game,index):
    with (game/'Media/san11pkres.bin').open('rb') as f:
        h=f.read(16)
        if h[:4]!=b'LINK' or index>=struct.unpack_from('<I',h,4)[0]:raise ValueError('not a valid resource pack')
        f.seek(16+index*8);at,size=struct.unpack('<II',f.read(8));f.seek(at);return f.read(size)


def export_native(m,game,out):
    template=resource(game,0x8ad)
    if template[:8]!=b'WKMD0010' or struct.unpack_from('<IIII',template,80)!=(0x11a,44,414,1000):
        raise ValueError('unexpected native boat; export aborted')
    ts=m.triangles();count=len(ts)*3
    if count>65535:raise ValueError('16-bit native vertex limit')
    data=bytearray(template[:1000]);mats=list(COLORS)
    # One opaque triangle-list batch, attached to original static hull bone 5.
    struct.pack_into('<I',data,76,0)  # no translucent batches
    struct.pack_into('<I',data,88,count)
    for points,n,mat in ts:
        uv=((mats.index(mat)+.5)/16,.5)
        for v in points:data+=struct.pack('<11f',*v,1,0,0,*n,*uv)
    struct.pack_into('<I',data,96,len(data))
    data+=b'\xff\xff\xff\xff'*count  # diffuse colour stream
    struct.pack_into('<II',data,116,count,len(data))
    data+=struct.pack('<'+'H'*count,*range(count))
    struct.pack_into('<IIIII',data,192,4,0,count,0,len(ts))
    xyz=np.array([v for p,n,mat in ts for v in p]);struct.pack_into('<6f',data,24,*xyz.min(axis=0),*xyz.max(axis=0))
    struct.pack_into('<I',data,8,len(data))
    (out/'merchant_ship.wkm').write_bytes(data)
    pixels=bytearray()
    for y in range(16):
        for x in range(256):
            c=COLORS[mats[min(x//16,len(mats)-1)]];pixels+=bytes([c[2],c[1],c[0],255])
    tex=b'WFTX0010'+struct.pack('<IIHHI',24+len(pixels),1,256,16,32)+pixels
    (out/'merchant_ship.wft').write_bytes(tex)


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--game',type=Path)
    a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    if a.game and (a.out.resolve()==a.game.resolve() or a.game.resolve() in a.out.resolve().parents):raise ValueError('output must be outside game')
    m=build_mesh();export_obj(m,a.out);export_glb(m,a.out)
    if a.game:export_native(m,a.game,a.out)
    xyz=np.array([v for p,n,mat in m.triangles() for v in p]);meta=dict(parts={k:len(v) for k,v in m.parts.items()},triangles=len(m.triangles()),vertices=len(xyz),bounds=[xyz.min(axis=0).tolist(),xyz.max(axis=0).tolist()],up_axis='Y',native_status='native export; runtime binding in trade_ship_visual.cpp; validate regenerated assets before deployment',historical_status='game art inspired by wooden merchant vessels; not an archaeological reconstruction')
    meta['visual_scale']=VISUAL_SCALE
    meta['design']='Han-inspired two-masted merchant; two single-tier sails, layered shelters and open cargo deck'
    meta['historical_status']='interpretive game reconstruction inspired by Han boat models; exact sea-going hull proportions and two-mast rig are conjectural'
    (a.out/'model.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(meta))


if __name__=='__main__':main()
