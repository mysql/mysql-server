//>>built
define("dojox/encoding/crypto/SimpleAES",["../base64","./_base"],function(_1,_2){
var _3=[99,124,119,123,242,107,111,197,48,1,103,43,254,215,171,118,202,130,201,125,250,89,71,240,173,212,162,175,156,164,114,192,183,253,147,38,54,63,247,204,52,165,229,241,113,216,49,21,4,199,35,195,24,150,5,154,7,18,128,226,235,39,178,117,9,131,44,26,27,110,90,160,82,59,214,179,41,227,47,132,83,209,0,237,32,252,177,91,106,203,190,57,74,76,88,207,208,239,170,251,67,77,51,133,69,249,2,127,80,60,159,168,81,163,64,143,146,157,56,245,188,182,218,33,16,255,243,210,205,12,19,236,95,151,68,23,196,167,126,61,100,93,25,115,96,129,79,220,34,42,144,136,70,238,184,20,222,94,11,219,224,50,58,10,73,6,36,92,194,211,172,98,145,149,228,121,231,200,55,109,141,213,78,169,108,86,244,234,101,122,174,8,186,120,37,46,28,166,180,198,232,221,116,31,75,189,139,138,112,62,181,102,72,3,246,14,97,53,87,185,134,193,29,158,225,248,152,17,105,217,142,148,155,30,135,233,206,85,40,223,140,161,137,13,191,230,66,104,65,153,45,15,176,84,187,22];
var _4=[[0,0,0,0],[1,0,0,0],[2,0,0,0],[4,0,0,0],[8,0,0,0],[16,0,0,0],[32,0,0,0],[64,0,0,0],[128,0,0,0],[27,0,0,0],[54,0,0,0]];
function _5(_6,w){
var Nb=4;
var Nr=w.length/Nb-1;
var _7=[[],[],[],[]];
for(var i=0;i<4*Nb;i++){
_7[i%4][Math.floor(i/4)]=_6[i];
}
_7=_8(_7,w,0,Nb);
for(var _9=1;_9<Nr;_9++){
_7=_a(_7,Nb);
_7=_b(_7,Nb);
_7=_c(_7,Nb);
_7=_8(_7,w,_9,Nb);
}
_7=_a(_7,Nb);
_7=_b(_7,Nb);
_7=_8(_7,w,Nr,Nb);
var _d=new Array(4*Nb);
for(var i=0;i<4*Nb;i++){
_d[i]=_7[i%4][Math.floor(i/4)];
}
return _d;
};
function _a(s,Nb){
for(var r=0;r<4;r++){
for(var c=0;c<Nb;c++){
s[r][c]=_3[s[r][c]];
}
}
return s;
};
function _b(s,Nb){
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
function _c(s,Nb){
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
function _8(_e,w,_f,Nb){
for(var r=0;r<4;r++){
for(var c=0;c<Nb;c++){
_e[r][c]^=w[_f*4+c][r];
}
}
return _e;
};
function _10(key){
var Nb=4;
var Nk=key.length/4;
var Nr=Nk+6;
var w=new Array(Nb*(Nr+1));
var _11=new Array(4);
for(var i=0;i<Nk;i++){
var r=[key[4*i],key[4*i+1],key[4*i+2],key[4*i+3]];
w[i]=r;
}
for(var i=Nk;i<(Nb*(Nr+1));i++){
w[i]=new Array(4);
for(var t=0;t<4;t++){
_11[t]=w[i-1][t];
}
if(i%Nk==0){
_11=_12(_13(_11));
for(var t=0;t<4;t++){
_11[t]^=_4[i/Nk][t];
}
}else{
if(Nk>6&&i%Nk==4){
_11=_12(_11);
}
}
for(var t=0;t<4;t++){
w[i][t]=w[i-Nk][t]^_11[t];
}
}
return w;
};
function _12(w){
for(var i=0;i<4;i++){
w[i]=_3[w[i]];
}
return w;
};
function _13(w){
w[4]=w[0];
for(var i=0;i<4;i++){
w[i]=w[i+1];
}
return w;
};
function _14(_15,_16,_17){
if(!(_17==128||_17==192||_17==256)){
return "";
}
var _18=_17/8;
var _19=new Array(_18);
for(var i=0;i<_18;i++){
_19[i]=_16.charCodeAt(i)&255;
}
var key=_5(_19,_10(_19));
key=key.concat(key.slice(0,_18-16));
var _1a=16;
var _1b=new Array(_1a);
var _1c=(new Date()).getTime();
for(var i=0;i<4;i++){
_1b[i]=(_1c>>>i*8)&255;
}
for(var i=0;i<4;i++){
_1b[i+4]=(_1c/4294967296>>>i*8)&255;
}
var _1d=_10(key);
var _1e=Math.ceil(_15.length/_1a);
var _1f=new Array(_1e);
for(var b=0;b<_1e;b++){
for(var c=0;c<4;c++){
_1b[15-c]=(b>>>c*8)&255;
}
for(var c=0;c<4;c++){
_1b[15-c-4]=(b/4294967296>>>c*8);
}
var _20=_5(_1b,_1d);
var _21=b<_1e-1?_1a:(_15.length-1)%_1a+1;
var ct="";
for(var i=0;i<_21;i++){
var _22=_15.charCodeAt(b*_1a+i);
var _23=_22^_20[i];
ct+=((_23<16)?"0":"")+_23.toString(16);
}
_1f[b]=ct;
}
var _24="";
for(var i=0;i<8;i++){
_24+=((_1b[i]<16)?"0":"")+_1b[i].toString(16);
}
return _24+" "+_1f.join(" ");
};
function _25(s){
var ret=[];
s.replace(/(..)/g,function(str){
ret.push(parseInt(str,16));
});
return ret;
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
var _2c=_10(_2b);
var key=_5(_2b,_2c);
key=key.concat(key.slice(0,_2a-16));
var _2d=_10(key);
_27=_27.split(" ");
var _2e=16;
var _2f=new Array(_2e);
var _30=_27[0];
_2f=_25(_30);
var _31=new Array(_27.length-1);
for(var b=1;b<_27.length;b++){
for(var c=0;c<4;c++){
_2f[15-c]=((b-1)>>>c*8)&255;
}
for(var c=0;c<4;c++){
_2f[15-c-4]=((b/4294967296-1)>>>c*8)&255;
}
var _32=_5(_2f,_2d);
var pt="";
var tmp=_25(_27[b]);
for(var i=0;i<tmp.length;i++){
var _33=_27[b].charCodeAt(i);
var _34=tmp[i]^_32[i];
pt+=String.fromCharCode(_34);
}
_31[b-1]=pt;
}
return _31.join("");
};
function _35(str){
return str.replace(/[\0\t\n\v\f\r\xa0!-]/g,function(c){
return "!"+c.charCodeAt(0)+"!";
});
};
function _36(str){
return str.replace(/!\d\d?\d?!/g,function(c){
return String.fromCharCode(c.slice(1,-1));
});
};
_2.SimpleAES=new (function(){
this.encrypt=function(_37,key){
return _14(_37,key,256);
};
this.decrypt=function(_38,key){
return _26(_38,key,256);
};
})();
return _2.SimpleAES;
});
