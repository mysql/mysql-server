/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/array",["./kernel","../has","./lang"],function(_1,_2,_3){
var _4={},u,_5;
function _6(){
_4={};
};
function _7(fn){
return _4[fn]=new Function("item","index","array",fn);
};
function _8(_9){
var _a=!_9;
return function(a,fn,o){
var i=0,l=a&&a.length||0,_b;
if(l&&typeof a=="string"){
a=a.split("");
}
if(typeof fn=="string"){
fn=_4[fn]||_7(fn);
}
if(o){
for(;i<l;++i){
_b=!fn.call(o,a[i],i,a);
if(_9^_b){
return !_b;
}
}
}else{
for(;i<l;++i){
_b=!fn(a[i],i,a);
if(_9^_b){
return !_b;
}
}
}
return _a;
};
};
function _c(up){
var _d=1,_e=0,_f=0;
if(!up){
_d=_e=_f=-1;
}
return function(a,x,_10,_11){
if(_11&&_d>0){
return _5.lastIndexOf(a,x,_10);
}
var l=a&&a.length||0,end=up?l+_f:_e,i;
if(_10===u){
i=up?_e:l+_f;
}else{
if(_10<0){
i=l+_10;
if(i<0){
i=_e;
}
}else{
i=_10>=l?l+_f:_10;
}
}
if(l&&typeof a=="string"){
a=a.split("");
}
for(;i!=end;i+=_d){
if(a[i]==x){
return i;
}
}
return -1;
};
};
function _12(a,fn,o){
var i=0,l=a&&a.length||0;
if(l&&typeof a=="string"){
a=a.split("");
}
if(typeof fn=="string"){
fn=_4[fn]||_7(fn);
}
if(o){
for(;i<l;++i){
fn.call(o,a[i],i,a);
}
}else{
for(;i<l;++i){
fn(a[i],i,a);
}
}
};
function map(a,fn,o,Ctr){
var i=0,l=a&&a.length||0,out=new (Ctr||Array)(l);
if(l&&typeof a=="string"){
a=a.split("");
}
if(typeof fn=="string"){
fn=_4[fn]||_7(fn);
}
if(o){
for(;i<l;++i){
out[i]=fn.call(o,a[i],i,a);
}
}else{
for(;i<l;++i){
out[i]=fn(a[i],i,a);
}
}
return out;
};
function _13(a,fn,o){
var i=0,l=a&&a.length||0,out=[],_14;
if(l&&typeof a=="string"){
a=a.split("");
}
if(typeof fn=="string"){
fn=_4[fn]||_7(fn);
}
if(o){
for(;i<l;++i){
_14=a[i];
if(fn.call(o,_14,i,a)){
out.push(_14);
}
}
}else{
for(;i<l;++i){
_14=a[i];
if(fn(_14,i,a)){
out.push(_14);
}
}
}
return out;
};
_5={every:_8(false),some:_8(true),indexOf:_c(true),lastIndexOf:_c(false),forEach:_12,map:map,filter:_13,clearCache:_6};
1&&_3.mixin(_1,_5);
return _5;
});
