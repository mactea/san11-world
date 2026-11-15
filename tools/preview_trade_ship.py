"""Render the actual mesh for inspection and build a self-contained WebGL viewer."""
import argparse
import base64
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from build_trade_ship_model import build_mesh, COLORS, VISUAL_SCALE


def render(mesh, yaw, elevation, size=900):
    triangles=mesh.triangles()
    eye=np.array([np.sin(yaw)*np.cos(elevation),np.sin(elevation),np.cos(yaw)*np.cos(elevation)])
    right=np.cross([0,1,0],eye);right/=np.linalg.norm(right)
    up=np.cross(eye,right);basis=np.array([right,up,eye])
    pixels=np.full((size,size,3),(25,37,43),dtype=np.uint8)
    depth=np.full((size,size),-np.inf)
    light=np.array([-.5,.8,.6]);light/=np.linalg.norm(light)
    for points,normal,mat in triangles:
        if normal@eye<=0:continue
        p=(points-[0,3.8*VISUAL_SCALE,0])@basis.T
        p[:,:2]*=size/(17*VISUAL_SCALE);p[:,0]+=size/2;p[:,1]=size/2-p[:,1]
        xmin=max(0,int(np.floor(p[:,0].min())));xmax=min(size-1,int(np.ceil(p[:,0].max())))
        ymin=max(0,int(np.floor(p[:,1].min())));ymax=min(size-1,int(np.ceil(p[:,1].max())))
        if xmin>xmax or ymin>ymax:continue
        x,y=np.meshgrid(np.arange(xmin,xmax+1)+.5,np.arange(ymin,ymax+1)+.5)
        a,b,c=p;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-9:continue
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;w=1-u-v
        z=u*a[2]+v*b[2]+w*c[2];region=depth[ymin:ymax+1,xmin:xmax+1]
        mask=(u>=0)&(v>=0)&(w>=0)&(z>region)
        color=np.array(COLORS[mat])*(.48+.52*max(0,normal@light))
        region[mask]=z[mask];pixels[ymin:ymax+1,xmin:xmax+1][mask]=color.astype(np.uint8)
    return Image.fromarray(pixels)


def viewer(mesh,out):
    rows=[]
    for points,n,mat in mesh.triangles():
        for p in points:rows.extend([*p,*n,*(v/255 for v in COLORS[mat])])
    encoded=base64.b64encode(np.array(rows,dtype='<f4').tobytes()).decode()
    html='''<!doctype html><meta charset="utf-8"><title>贸易商船 · 3D 模型</title>
<style>html,body{margin:0;height:100%;background:#19252b;color:#e8ddc8;font:15px system-ui}canvas{width:100%;height:100%;display:block;touch-action:none}aside{position:absolute;left:28px;top:22px;pointer-events:none}h1{font-size:24px;margin:0 0 8px}p{opacity:.75;margin:5px 0}button{pointer-events:auto;background:#354c55;border:1px solid #687c7e;color:inherit;padding:8px 16px;margin-top:12px;border-radius:5px}</style>
<canvas id="c"></canvas><aside><h1>汉风大型海贸船</h1><p>双桅双帆 · 宽厚船体 · 分段舱篷 · 开阔货运甲板</p><p>拖动旋转 · 滚轮缩放 · 双击复位</p><p>参考汉代船模的美术复原 · 帆装与比例含推测</p><button id="spin">自动旋转</button></aside>
<script>
const c=document.getElementById('c'),gl=c.getContext('webgl');
if(!gl){document.querySelector('aside').append('此浏览器无法使用 WebGL。');throw Error('WebGL unavailable');}
const vs=`attribute vec3 p,n,col;uniform vec3 right,up,eye;uniform float scale,aspect;varying vec3 color;void main(){vec3 q=p-vec3(0.,3.8,0.);gl_Position=vec4(dot(q,right)*scale/aspect,dot(q,up)*scale,-dot(q,eye)/40.,1.);color=col*(.48+.52*max(0.,dot(n,normalize(vec3(-.5,.8,.6)))));}`;
const fs=`precision mediump float;varying vec3 color;void main(){gl_FragColor=vec4(color,1.);}`;
function shader(type,src){const s=gl.createShader(type);gl.shaderSource(s,src);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(s));return s;}
const prog=gl.createProgram();gl.attachShader(prog,shader(gl.VERTEX_SHADER,vs));gl.attachShader(prog,shader(gl.FRAGMENT_SHADER,fs));gl.linkProgram(prog);if(!gl.getProgramParameter(prog,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(prog));gl.useProgram(prog);
const bytes=Uint8Array.from(atob('__DATA__'),x=>x.charCodeAt(0)),data=new Float32Array(bytes.buffer);
gl.bindBuffer(gl.ARRAY_BUFFER,gl.createBuffer());gl.bufferData(gl.ARRAY_BUFFER,data,gl.STATIC_DRAW);
['p','n','col'].forEach((k,i)=>{let a=gl.getAttribLocation(prog,k);gl.enableVertexAttribArray(a);gl.vertexAttribPointer(a,3,gl.FLOAT,false,36,i*12);});
const uniforms=Object.fromEntries(['right','up','eye','scale','aspect'].map(k=>[k,gl.getUniformLocation(prog,k)]));
gl.enable(gl.DEPTH_TEST);gl.enable(gl.CULL_FACE);gl.clearColor(.098,.145,.169,1);
let yaw=-.9,elev=.42,zoom=.115,drag=false,spin=false,last=0;
c.onpointerdown=e=>{drag=true;c.setPointerCapture(e.pointerId);};c.onpointerup=()=>drag=false;c.onpointercancel=()=>drag=false;
c.onpointermove=e=>{if(drag){yaw-=e.movementX*.008;elev=Math.max(-1.3,Math.min(1.3,elev+e.movementY*.008));}};
c.onwheel=e=>{e.preventDefault();zoom=Math.max(.045,Math.min(.35,zoom*Math.exp(-e.deltaY*.001)));};
c.ondblclick=()=>{yaw=-.9;elev=.42;zoom=.115;};document.getElementById('spin').onclick=()=>{spin=!spin;document.getElementById('spin').textContent=spin?'停止旋转':'自动旋转';};
function draw(t){if(spin&&!drag)yaw+=Math.min(100,t-last)*.0002;last=t;
const d=Math.min(2,devicePixelRatio||1),w=Math.round(c.clientWidth*d),h=Math.round(c.clientHeight*d);if(c.width!==w||c.height!==h){c.width=w;c.height=h;}gl.viewport(0,0,w,h);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
let sy=Math.sin(yaw),cy=Math.cos(yaw),se=Math.sin(elev),ce=Math.cos(elev);
gl.uniform3f(uniforms.right,cy,0,-sy);gl.uniform3f(uniforms.up,-sy*se,ce,-cy*se);gl.uniform3f(uniforms.eye,sy*ce,se,cy*ce);gl.uniform1f(uniforms.scale,zoom);gl.uniform1f(uniforms.aspect,w/h);gl.drawArrays(gl.TRIANGLES,0,data.length/9);requestAnimationFrame(draw);}requestAnimationFrame(draw);
</script>'''
    html=html.replace('3.8,0.',f'{3.8*VISUAL_SCALE:g},0.').replace('zoom=.115',f'zoom={.115/VISUAL_SCALE}')
    (out/'preview.html').write_text(html.replace('__DATA__',encoded),encoding='utf-8')


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--out',required=True,type=Path);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    mesh=build_mesh();viewer(mesh,a.out)
    sheet=Image.new('RGB',(1800,1800));draw=ImageDraw.Draw(sheet)
    for i,(yaw,elev,label) in enumerate([(-.9,.42,'STERN QUARTER'),(2.25,.38,'BOW QUARTER'),(-1.57,.12,'PROFILE'),(.4,1.15,'DECK / CARGO')]):
        x=(i%2)*900;y=(i//2)*900;sheet.paste(render(mesh,yaw,elev),(x,y));draw.text((x+25,y+25),label,fill=(220,210,180))
    sheet.save(a.out/'preview.png');print('Rendered four mesh views and standalone preview.html')


if __name__=='__main__':main()
