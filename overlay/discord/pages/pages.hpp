#pragma once

#include "includes/includes.hpp"

namespace discord::pages {
	inline const char* APP_HTML = R"html(<!doctype html><html><head><meta charset=utf-8><title>app</title></head>
<body style="background:#111;color:#9c9;font:14px monospace;padding:12px">
<h3>PoC ImGui Discord Overlay</h3>
<div id="s">Initializing...</div>
<script>
const PORT=__PORT__,PID=__PID__,K='DISCORD_IMGUI_OVERLAY';
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
  const s=document.getElementById('s');
  try{
    s.textContent='Waiting for Discord startup...';
    await sleep(4000);
    window.open('http://127.0.0.1:'+PORT+'/popout',K,
      'outOfProcessOverlay=true,focusable=false,width=1280,height=720');
    await sleep(1500);
    const DN=window.DiscordNative;
    await DN.window.setBackgroundThrottling(false);
    await DN.window.showInactive(K);
    if(PID){
      const h=String(await DN.window.getNativeHandle(K));
      await DN.nativeModules.ensureModule('discord_desktop_overlay');
      const m=DN.nativeModules.requireModule('discord_desktop_overlay');
      try{m.init()}catch(e){}
      m.setHostWindowCallbacks(async pid=>String(h),()=>{},p=>{});
      await m.trackGame(PID);
      s.textContent='Overlay attached to PID '+PID;
    }
    s.textContent='Overlay active';
  }catch(e){s.textContent='ERR: '+(e&&e.message||e)}
})();
</script></body></html>)html";

  inline const char* POPOUT_HTML = R"html(<!doctype html><html><head>
<meta charset=utf-8><title>POC WDA OVERLAY</title>
<style>*{margin:0;padding:0}html,body{width:100%;height:100%;overflow:hidden;background:transparent}canvas{display:block}</style>
</head><body>
<canvas id="c"></canvas>
<script>
(function(){
'use strict';
const canvas=document.getElementById('c');
canvas.width=window.innerWidth;canvas.height=window.innerHeight;
const gl=canvas.getContext('webgl',{alpha:true,premultipliedAlpha:false,antialias:false});
if(!gl){document.body.textContent='WebGL not supported';return;}

const vsSrc='precision mediump float;uniform mat4 P;attribute vec2 Pos;attribute vec2 UV;attribute vec4 Col;varying vec2 vUV;varying vec4 vCol;void main(){vUV=UV;vCol=Col;gl_Position=P*vec4(Pos,0,1);}';
const fsSrc='precision mediump float;uniform sampler2D T;varying vec2 vUV;varying vec4 vCol;void main(){gl_FragColor=vCol*texture2D(T,vUV.st);}';

function mkS(t,s){const o=gl.createShader(t);gl.shaderSource(o,s);gl.compileShader(o);return o;}
const prog=gl.createProgram();
gl.attachShader(prog,mkS(gl.VERTEX_SHADER,vsSrc));
gl.attachShader(prog,mkS(gl.FRAGMENT_SHADER,fsSrc));
gl.linkProgram(prog);

const uT=gl.getUniformLocation(prog,'T'),uP=gl.getUniformLocation(prog,'P');
const aPos=gl.getAttribLocation(prog,'Pos'),aUV=gl.getAttribLocation(prog,'UV'),aCol=gl.getAttribLocation(prog,'Col');
const vbo=gl.createBuffer(),ibo=gl.createBuffer();

gl.enable(gl.BLEND);gl.blendEquation(gl.FUNC_ADD);
gl.blendFuncSeparate(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA,gl.ONE,gl.ONE_MINUS_SRC_ALPHA);
gl.disable(gl.CULL_FACE);gl.disable(gl.DEPTH_TEST);

function updProj(){
  const w=canvas.width,h=canvas.height;
  gl.viewport(0,0,w,h);gl.useProgram(prog);
  gl.uniformMatrix4fv(uP,false,new Float32Array([2/w,0,0,0,0,-2/h,0,0,0,0,-1,0,-1,1,0,1]));
}
updProj();
gl.useProgram(prog);gl.uniform1i(uT,0);
gl.bindBuffer(gl.ARRAY_BUFFER,vbo);gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,ibo);
gl.enableVertexAttribArray(aPos);gl.enableVertexAttribArray(aUV);gl.enableVertexAttribArray(aCol);
gl.vertexAttribPointer(aPos,2,gl.FLOAT,false,20,0);
gl.vertexAttribPointer(aUV,2,gl.FLOAT,false,20,8);
gl.vertexAttribPointer(aCol,4,gl.UNSIGNED_BYTE,true,20,16);

const texMap={};
let lastTex=-1;
function procTex(data){
  const dv=new DataView(data);
  const id=dv.getUint32(1,true),type=dv.getInt32(5,true);
  const w=dv.getInt32(9,true),h=dv.getInt32(13,true);
  const raw=new Uint8Array(data,21);
  const rgba=new Uint8Array(4*w*h);
  if(type===0){const r32=new Uint32Array(rgba.buffer);for(let i=0;i<w*h;i++)r32[i]=0x00FFFFFF|(raw[i]<<24);}
  else if(type===1){for(let i=0;i<w*h;i++){rgba[4*i]=raw[i];rgba[4*i+1]=raw[i];rgba[4*i+2]=raw[i];rgba[4*i+3]=255;}}
  else if(type===3){rgba.set(raw.subarray(0,4*w*h));}
  if(!texMap[id])texMap[id]=gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D,texMap[id]);
  gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
  gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,w,h,0,gl.RGBA,gl.UNSIGNED_BYTE,rgba);
  lastTex=id;
}

function renderList(dv,data,base){
  let o=base;
  const nV=dv.getUint32(o,true);o+=4;
  if(nV===0)return;
  gl.bufferData(gl.ARRAY_BUFFER,new Uint8Array(data,o,20*nV),gl.STREAM_DRAW);
  o+=20*nV;
  const nI=dv.getUint32(o,true);o+=4;
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,new Uint8Array(data,o,2*nI),gl.STREAM_DRAW);
  o+=2*nI;
  const nC=dv.getUint32(o,true);o+=4;
  for(let c=0;c<nC;c++){
    const ne=dv.getUint32(o,true);o+=4;
    const tid=dv.getUint32(o,true);o+=4;
    o+=4;
    const io2=dv.getUint32(o,true);o+=4;
    const cx=dv.getFloat32(o,true);o+=4;
    const cy=dv.getFloat32(o,true);o+=4;
    const cz=dv.getFloat32(o,true);o+=4;
    const cw=dv.getFloat32(o,true);o+=4;
    if(cx<canvas.width&&cy<canvas.height&&cz>=0&&cw>=0){
      gl.scissor(cx,canvas.height-cw,cz-cx,cw-cy);
      if(tid!==lastTex&&texMap[tid]){gl.bindTexture(gl.TEXTURE_2D,texMap[tid]);lastTex=tid;}
      gl.drawElements(gl.TRIANGLES,ne,gl.UNSIGNED_SHORT,2*io2);
    }
  }
}

function renderFrame(data){
  gl.clearColor(0,0,0,0);gl.clear(gl.COLOR_BUFFER_BIT);
  const dv=new DataView(data);let o=1;
  const nL=dv.getUint32(o,true);o+=4;
  gl.enable(gl.SCISSOR_TEST);
  for(let i=0;i<nL;i++){
    const sz=dv.getUint32(o,true);o+=4;
    if(sz>0)renderList(dv,data,o);
    o+=sz;
  }
  gl.disable(gl.SCISSOR_TEST);
}

window.addEventListener('resize',()=>{
  canvas.width=window.innerWidth;canvas.height=window.innerHeight;
  updProj();
});

let ws,busy=false;
function reqFrame(){
  if(!ws||ws.readyState!==1||busy)return;
  busy=true;
  ws.send('frame');
}
function connect(){
  ws=new WebSocket('ws://127.0.0.1:__PORT__/ws');ws.binaryType='arraybuffer';
  ws.onopen=()=>{ws.send('init');};
  ws.onmessage=evt=>{
    if(!(evt.data instanceof ArrayBuffer)||evt.data.byteLength<1)return;
    const t=new Uint8Array(evt.data,0,1)[0];
    if(t===1){procTex(evt.data);reqFrame();}
    else if(t===2){renderFrame(evt.data);busy=false;requestAnimationFrame(reqFrame);}
  };
  ws.onclose=()=>{busy=false;setTimeout(connect,2000);};
  ws.onerror=()=>{};
}
connect();
})();
</script></body></html>)html";
} // namespace discord::pages
