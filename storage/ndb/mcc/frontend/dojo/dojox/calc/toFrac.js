//>>built
define("dojox/calc/toFrac",["dojo/_base/lang","dojox/calc/_Executor"],function(_1,_2){
var _3;
function _4(){
var _5=[5,6,7,10,11,13,14,15,17,19,21,22,23,26,29,30,31,33,34,35,37,38,39,41,42,43,46,47,51,53,55,57,58,59,61,62,65,66,67,69,70,71,73,74,77,78,79,82,83,85,86,87,89,91,93,94,95,97];
_3={"1":1,"√(2)":Math.sqrt(2),"√(3)":Math.sqrt(3),"pi":Math.PI};
for(var i in _5){
var n=_5[i];
_3["√("+n+")"]=Math.sqrt(n);
}
_3["√(pi)"]=Math.sqrt(Math.PI);
};
function _6(_7){
function _8(_9){
var _a=Math.floor(1/_9);
var _b=_2.approx(1/_a);
if(_b==_9){
return {n:1,d:_a};
}
var _c=_a+1;
_b=_2.approx(1/_c);
if(_b==_9){
return {n:1,d:_c};
}
if(_a>=50){
return null;
}
var _d=_a+_c;
_b=_2.approx(2/_d);
if(_b==_9){
return {n:2,d:_d};
}
if(_a>=34){
return null;
}
var _e=_9<_b;
var _f=_d*2+(_e?1:-1);
_b=_2.approx(4/_f);
if(_b==_9){
return {n:4,d:_f};
}
var _10=_9<_b;
if((_e&&!_10)||(!_e&&_10)){
var _11=(_d+_f)>>1;
_b=_2.approx(3/_11);
if(_b==_9){
return {n:3,d:_11};
}
}
if(_a>=20){
return null;
}
var _12=_d+_a*2;
var _13=_12+2;
for(var _14=5;_12<=100;_14++){
_12+=_a;
_13+=_c;
var _15=_e?((_13+_12+1)>>1):_12;
var _16=_e?_13:((_13+_12-1)>>1);
_15=_10?((_15+_16)>>1):_15;
_16=_10?_16:((_15+_16)>>1);
for(var _17=_15;_17<=_16;_17++){
if(_14&1==0&&_17&1==0){
continue;
}
_b=_2.approx(_14/_17);
if(_b==_9){
return {n:_14,d:_17};
}
if(_b<_9){
break;
}
}
}
return null;
};
_7=Math.abs(_7);
for(var mt in _3){
var _18=_3[mt];
var _19=_7/_18;
var _1a=Math.floor(_19);
_19=_2.approx(_19-_1a);
if(_19==0){
return {mt:mt,m:_18,n:_1a,d:1};
}else{
var a=_8(_19);
if(!a){
continue;
}
return {mt:mt,m:_18,n:(_1a*a.d+a.n),d:a.d};
}
}
return null;
};
_4();
return _1.mixin(_2,{toFrac:function(_1b){
var f=_6(_1b);
return f?((_1b<0?"-":"")+(f.m==1?"":(f.n==1?"":(f.n+"*")))+(f.m==1?f.n:f.mt)+((f.d==1?"":"/"+f.d))):_1b;
},pow:function(_1c,_1d){
function _1e(n){
return Math.floor(n)==n;
};
if(_1c>0||_1e(_1d)){
return Math.pow(_1c,_1d);
}else{
var f=_6(_1d);
if(_1c>=0){
return (f&&f.m==1)?Math.pow(Math.pow(_1c,1/f.d),_1d<0?-f.n:f.n):Math.pow(_1c,_1d);
}else{
return (f&&f.d&1)?Math.pow(Math.pow(-Math.pow(-_1c,1/f.d),_1d<0?-f.n:f.n),f.m):NaN;
}
}
}});
});
