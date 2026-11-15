"""Validate delivered GLB/WKMD/WFT geometry and cross-format consistency."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np


def check(ok,message):
    if not ok:raise ValueError(message)
    print('PASS '+message)


def validate(folder):
    glb=(folder/'merchant_ship.glb').read_bytes()
    magic,version,length=struct.unpack_from('<4sII',glb)
    check(magic==b'glTF' and version==2 and length==len(glb),'GLB 2.0 container')
    size,kind=struct.unpack_from('<I4s',glb,12);doc=json.loads(glb[20:20+size]);at=20+size
    bsize,bkind=struct.unpack_from('<I4s',glb,at);data=glb[at+8:]
    check(kind==b'JSON' and bkind==b'BIN\0' and bsize==len(data),'GLB chunk lengths')
    positions=[];normals=[]
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            check(primitive['mode']==4 and primitive['material']<len(doc['materials']),mesh['name']+' triangle/material reference')
            for semantic,target in [('POSITION',positions),('NORMAL',normals)]:
                a=doc['accessors'][primitive['attributes'][semantic]];v=doc['bufferViews'][a['bufferView']]
                check(a['componentType']==5126 and a['type']=='VEC3' and v['byteLength']==a['count']*12 and v['byteOffset']+v['byteLength']<=len(data),semantic+' accessor bounds')
                arr=np.frombuffer(data,dtype='<f4',count=a['count']*3,offset=v['byteOffset']).reshape(-1,3);target.append(arr)
                if semantic=='POSITION':check(np.allclose(a['min'],arr.min(0)) and np.allclose(a['max'],arr.max(0)),'GLB accessor bounding box')
    xyz=np.concatenate(positions);n=np.concatenate(normals);tri=xyz.reshape(-1,3,3)
    cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);areas=np.linalg.norm(cross,axis=1)
    check(np.isfinite(xyz).all() and np.isfinite(n).all() and np.all(areas>1e-9),'finite non-degenerate geometry')
    check(np.allclose(np.linalg.norm(n,axis=1),1,atol=1e-5) and np.all(np.sum(cross*n[::3],axis=1)>0),'normals agree with triangle winding')
    native=(folder/'merchant_ship.wkm').read_bytes();u=lambda o:struct.unpack_from('<I',native,o)[0]
    check(native[:8]==b'WKMD0010' and u(8)==len(native) and u(68)==1,'WKMD header')
    check(u(80)==0x11a and u(84)==44 and u(88)==len(xyz) and len(xyz)<=65535,'WKMD weighted vertex layout')
    vertices=np.frombuffer(native,dtype='<f4',count=u(88)*11,offset=u(92)).reshape(-1,11)
    check(u(96)==u(92)+len(vertices)*44 and u(116)==len(xyz) and u(120)+u(116)*2==len(native),'WKMD stream boundaries')
    indices=np.frombuffer(native,dtype='<u2',count=u(116),offset=u(120))
    check(np.all(indices<len(vertices)) and u(192)==4 and u(200)==len(xyz) and u(208)==len(xyz)//3,'WKMD triangle-list draw range')
    check(np.allclose(vertices[:,3:6],[1,0,0]),'static hull bone weighting')
    # GLB groups each part by material, so compare multisets of vertex coordinates.
    a=sorted(map(tuple,np.round(xyz,5)));b=sorted(map(tuple,np.round(vertices[:,:3],5)))
    check(a==b,'GLB and WKMD contain the same geometry')
    bounds=struct.unpack_from('<6f',native,24)
    check(np.allclose(bounds[:3],xyz.min(0)) and np.allclose(bounds[3:],xyz.max(0)),'WKMD bounding box')
    texture=(folder/'merchant_ship.wft').read_bytes()
    check(texture[:8]==b'WFTX0010' and struct.unpack_from('<IIHHI',texture,8)==(len(texture),1,256,16,32) and len(texture)==24+256*16*4,'WFT BGRA texture layout')
    obj=(folder/'merchant_ship.obj').read_text()
    check(sum(line.startswith('v ') for line in obj.splitlines())==len(xyz) and sum(line.startswith('f ') for line in obj.splitlines())==len(xyz)//3,'OBJ geometry counts')
    report=dict(status='passed',triangles=len(xyz)//3,vertices=len(xyz),parts=len(doc['meshes']),
                files={name:hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in ['merchant_ship.glb','merchant_ship.obj','merchant_ship.mtl','merchant_ship.wkm','merchant_ship.wft']})
    (folder/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('folder',type=Path);validate(parser.parse_args().folder)
