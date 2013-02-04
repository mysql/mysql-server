//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.sql._crypto");
_2.mixin(_3.sql._crypto,{_POOL_SIZE:100,encrypt:function(_4,_5,_6){
this._initWorkerPool();
var _7={plaintext:_4,password:_5};
_7=_2.toJson(_7);
_7="encr:"+String(_7);
this._assignWork(_7,_6);
},decrypt:function(_8,_9,_a){
this._initWorkerPool();
var _b={ciphertext:_8,password:_9};
_b=_2.toJson(_b);
_b="decr:"+String(_b);
this._assignWork(_b,_a);
},_initWorkerPool:function(){
if(!this._manager){
try{
this._manager=google.gears.factory.create("beta.workerpool","1.0");
this._unemployed=[];
this._employed={};
this._handleMessage=[];
var _c=this;
this._manager.onmessage=function(_d,_e){
var _f=_c._employed["_"+_e];
_c._employed["_"+_e]=undefined;
_c._unemployed.push("_"+_e);
if(_c._handleMessage.length){
var _10=_c._handleMessage.shift();
_c._assignWork(_10.msg,_10.callback);
}
_f(_d);
};
var _11="function _workerInit(){"+"gearsWorkerPool.onmessage = "+String(this._workerHandler)+";"+"}";
var _12=_11+" _workerInit();";
for(var i=0;i<this._POOL_SIZE;i++){
this._unemployed.push("_"+this._manager.createWorker(_12));
}
}
catch(exp){
throw exp.message||exp;
}
}
},_assignWork:function(msg,_13){
if(!this._handleMessage.length&&this._unemployed.length){
var _14=this._unemployed.shift().substring(1);
this._employed["_"+_14]=_13;
this._manager.sendMessage(msg,parseInt(_14,10));
}else{
this._handleMessage={msg:msg,callback:_13};
}
},_workerHandler:function(msg,_15){
var _16=[99,124,119,123,242,107,111,197,48,1,103,43,254,215,171,118,202,130,201,125,250,89,71,240,173,212,162,175,156,164,114,192,183,253,147,38,54,63,247,204,52,165,229,241,113,216,49,21,4,199,35,195,24,150,5,154,7,18,128,226,235,39,178,117,9,131,44,26,27,110,90,160,82,59,214,179,41,227,47,132,83,209,0,237,32,252,177,91,106,203,190,57,74,76,88,207,208,239,170,251,67,77,51,133,69,249,2,127,80,60,159,168,81,163,64,143,146,157,56,245,188,182,218,33,16,255,243,210,205,12,19,236,95,151,68,23,196,167,126,61,100,93,25,115,96,129,79,220,34,42,144,136,70,238,184,20,222,94,11,219,224,50,58,10,73,6,36,92,194,211,172,98,145,149,228,121,231,200,55,109,141,213,78,169,108,86,244,234,101,122,174,8,186,120,37,46,28,166,180,198,232,221,116,31,75,189,139,138,112,62,181,102,72,3,246,14,97,53,87,185,134,193,29,158,225,248,152,17,105,217,142,148,155,30,135,233,206,85,40,223,140,161,137,13,191,230,66,104,65,153,45,15,176,84,187,22];
var _17=[[0,0,0,0],[1,0,0,0],[2,0,0,0],[4,0,0,0],[8,0,0,0],[16,0,0,0],[32,0,0,0],[64,0,0,0],[128,0,0,0],[27,0,0,0],[54,0,0,0]];
function _18(_19,w){
var Nb=4;
var Nr=w.length/Nb-1;
var _1a=[[],[],[],[]];
for(var i=0;i<4*Nb;i++){
_1a[i%4][Math.floor(i/4)]=_19[i];
}
_1a=_1b(_1a,w,0,Nb);
for(var _1c=1;_1c<Nr;_1c++){
_1a=_1d(_1a,Nb);
_1a=_1e(_1a,Nb);
_1a=_1f(_1a,Nb);
_1a=_1b(_1a,w,_1c,Nb);
}
_1a=_1d(_1a,Nb);
_1a=_1e(_1a,Nb);
_1a=_1b(_1a,w,Nr,Nb);
var _20=new Array(4*Nb);
for(var i=0;i<4*Nb;i++){
_20[i]=_1a[i%4][Math.floor(i/4)];
}
return _20;
};
function _1d(s,Nb){
for(var r=0;r<4;r++){
for(var c=0;c<Nb;c++){
s[r][c]=_16[s[r][c]];
}
}
return s;
};
function _1e(s,Nb){
var t=new Array(4);
for(var r=1;r<4;r++){
for(var c=0;c<4;c++){
t[c]=s[r][(c+r)%Nb];
}
for(var c=0;c<4;c++){
s[r][c]=t[c];
}
}
return s;
};
function _1f(s,Nb){
for(var c=0;c<4;c++){
var a=new Array(4);
var b=new Array(4);
for(var i=0;i<4;i++){
a[i]=s[i][c];
b[i]=s[i][c]&128?s[i][c]<<1^283:s[i][c]<<1;
}
s[0][c]=b[0]^a[1]^b[1]^a[2]^a[3];
s[1][c]=a[0]^b[1]^a[2]^b[2]^a[3];
s[2][c]=a[0]^a[1]^b[2]^a[3]^b[3];
s[3][c]=a[0]^b[0]^a[1]^a[2]^b[3];
}
return s;
};
function _1b(_21,w,rnd,Nb){
for(var r=0;r<4;r++){
for(var c=0;c<Nb;c++){
_21[r][c]^=w[rnd*4+c][r];
}
}
return _21;
};
function _22(key){
var Nb=4;
var Nk=key.length/4;
var Nr=Nk+6;
var w=new Array(Nb*(Nr+1));
var _23=new Array(4);
for(var i=0;i<Nk;i++){
var r=[key[4*i],key[4*i+1],key[4*i+2],key[4*i+3]];
w[i]=r;
}
for(var i=Nk;i<(Nb*(Nr+1));i++){
w[i]=new Array(4);
for(var t=0;t<4;t++){
_23[t]=w[i-1][t];
}
if(i%Nk==0){
_23=_24(_25(_23));
for(var t=0;t<4;t++){
_23[t]^=_17[i/Nk][t];
}
}else{
if(Nk>6&&i%Nk==4){
_23=_24(_23);
}
}
for(var t=0;t<4;t++){
w[i][t]=w[i-Nk][t]^_23[t];
}
}
return w;
};
function _24(w){
for(var i=0;i<4;i++){
w[i]=_16[w[i]];
}
return w;
};
function _25(w){
w[4]=w[0];
for(var i=0;i<4;i++){
w[i]=w[i+1];
}
return w;
};
function _26(_27,_28,_29){
if(!(_29==128||_29==192||_29==256)){
return "";
}
var _2a=_29/8;
var _2b=new Array(_2a);
for(var i=0;i<_2a;i++){
_2b[i]=_28.charCodeAt(i)&255;
}
var key=_18(_2b,_22(_2b));
key=key.concat(key.slice(0,_2a-16));
var _2c=16;
var _2d=new Array(_2c);
var _2e=(new Date()).getTime();
for(var i=0;i<4;i++){
_2d[i]=(_2e>>>i*8)&255;
}
for(var i=0;i<4;i++){
_2d[i+4]=(_2e/4294967296>>>i*8)&255;
}
var _2f=_22(key);
var _30=Math.ceil(_27.length/_2c);
var _31=new Array(_30);
for(var b=0;b<_30;b++){
for(var c=0;c<4;c++){
_2d[15-c]=(b>>>c*8)&255;
}
for(var c=0;c<4;c++){
_2d[15-c-4]=(b/4294967296>>>c*8);
}
var _32=_18(_2d,_2f);
var _33=b<_30-1?_2c:(_27.length-1)%_2c+1;
var ct="";
for(var i=0;i<_33;i++){
var _34=_27.charCodeAt(b*_2c+i);
var _35=_34^_32[i];
ct+=String.fromCharCode(_35);
}
_31[b]=_36(ct);
}
var _37="";
for(var i=0;i<8;i++){
_37+=String.fromCharCode(_2d[i]);
}
_37=_36(_37);
return _37+"-"+_31.join("-");
};
function _38(_39,_3a,_3b){
if(!(_3b==128||_3b==192||_3b==256)){
return "";
}
var _3c=_3b/8;
var _3d=new Array(_3c);
for(var i=0;i<_3c;i++){
_3d[i]=_3a.charCodeAt(i)&255;
}
var _3e=_22(_3d);
var key=_18(_3d,_3e);
key=key.concat(key.slice(0,_3c-16));
var _3f=_22(key);
_39=_39.split("-");
var _40=16;
var _41=new Array(_40);
var _42=_43(_39[0]);
for(var i=0;i<8;i++){
_41[i]=_42.charCodeAt(i);
}
var _44=new Array(_39.length-1);
for(var b=1;b<_39.length;b++){
for(var c=0;c<4;c++){
_41[15-c]=((b-1)>>>c*8)&255;
}
for(var c=0;c<4;c++){
_41[15-c-4]=((b/4294967296-1)>>>c*8)&255;
}
var _45=_18(_41,_3f);
_39[b]=_43(_39[b]);
var pt="";
for(var i=0;i<_39[b].length;i++){
var _46=_39[b].charCodeAt(i);
var _47=_46^_45[i];
pt+=String.fromCharCode(_47);
}
_44[b-1]=pt;
}
return _44.join("");
};
function _36(str){
return str.replace(/[\0\t\n\v\f\r\xa0!-]/g,function(c){
return "!"+c.charCodeAt(0)+"!";
});
};
function _43(str){
return str.replace(/!\d\d?\d?!/g,function(c){
return String.fromCharCode(c.slice(1,-1));
});
};
function _48(_49,_4a){
return _26(_49,_4a,256);
};
function _4b(_4c,_4d){
return _38(_4c,_4d,256);
};
var cmd=msg.substr(0,4);
var arg=msg.substr(5);
if(cmd=="encr"){
arg=eval("("+arg+")");
var _4e=arg.plaintext;
var _4f=arg.password;
var _50=_48(_4e,_4f);
gearsWorkerPool.sendMessage(String(_50),_15);
}else{
if(cmd=="decr"){
arg=eval("("+arg+")");
var _51=arg.ciphertext;
var _4f=arg.password;
var _50=_4b(_51,_4f);
gearsWorkerPool.sendMessage(String(_50),_15);
}
}
}});
});
