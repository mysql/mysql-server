/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/lang",["./kernel","../has","../sniff"],function(_1,_2){
_2.add("bug-for-in-skips-shadowed",function(){
for(var i in {toString:1}){
return 0;
}
return 1;
});
var _3=_2("bug-for-in-skips-shadowed")?"hasOwnProperty.valueOf.isPrototypeOf.propertyIsEnumerable.toLocaleString.toString.constructor".split("."):[],_4=_3.length,_5=function(_6,_7,_8){
if(!_8){
if(_6[0]&&_1.scopeMap[_6[0]]){
_8=_1.scopeMap[_6.shift()][1];
}else{
_8=_1.global;
}
}
try{
for(var i=0;i<_6.length;i++){
var p=_6[i];
if(!(p in _8)){
if(_7){
_8[p]={};
}else{
return;
}
}
_8=_8[p];
}
return _8;
}
catch(e){
}
},_9=Object.prototype.toString,_a=function(_b,_c,_d){
return (_d||[]).concat(Array.prototype.slice.call(_b,_c||0));
},_e=/\{([^\}]+)\}/g;
var _f={_extraNames:_3,_mixin:function(_10,_11,_12){
var _13,s,i,_14={};
for(_13 in _11){
s=_11[_13];
if(!(_13 in _10)||(_10[_13]!==s&&(!(_13 in _14)||_14[_13]!==s))){
_10[_13]=_12?_12(s):s;
}
}
if(_2("bug-for-in-skips-shadowed")){
if(_11){
for(i=0;i<_4;++i){
_13=_3[i];
s=_11[_13];
if(!(_13 in _10)||(_10[_13]!==s&&(!(_13 in _14)||_14[_13]!==s))){
_10[_13]=_12?_12(s):s;
}
}
}
}
return _10;
},mixin:function(_15,_16){
if(!_15){
_15={};
}
for(var i=1,l=arguments.length;i<l;i++){
_f._mixin(_15,arguments[i]);
}
return _15;
},setObject:function(_17,_18,_19){
var _1a=_17.split("."),p=_1a.pop(),obj=_5(_1a,true,_19);
return obj&&p?(obj[p]=_18):undefined;
},getObject:function(_1b,_1c,_1d){
return !_1b?_1d:_5(_1b.split("."),_1c,_1d);
},exists:function(_1e,obj){
return _f.getObject(_1e,false,obj)!==undefined;
},isString:function(it){
return (typeof it=="string"||it instanceof String);
},isArray:Array.isArray||function(it){
return _9.call(it)=="[object Array]";
},isFunction:function(it){
return _9.call(it)==="[object Function]";
},isObject:function(it){
return it!==undefined&&(it===null||typeof it=="object"||_f.isArray(it)||_f.isFunction(it));
},isArrayLike:function(it){
return !!it&&!_f.isString(it)&&!_f.isFunction(it)&&!(it.tagName&&it.tagName.toLowerCase()=="form")&&(_f.isArray(it)||isFinite(it.length));
},isAlien:function(it){
return it&&!_f.isFunction(it)&&/\{\s*\[native code\]\s*\}/.test(String(it));
},extend:function(_1f,_20){
for(var i=1,l=arguments.length;i<l;i++){
_f._mixin(_1f.prototype,arguments[i]);
}
return _1f;
},_hitchArgs:function(_21,_22){
var pre=_f._toArray(arguments,2);
var _23=_f.isString(_22);
return function(){
var _24=_f._toArray(arguments);
var f=_23?(_21||_1.global)[_22]:_22;
return f&&f.apply(_21||this,pre.concat(_24));
};
},hitch:function(_25,_26){
if(arguments.length>2){
return _f._hitchArgs.apply(_1,arguments);
}
if(!_26){
_26=_25;
_25=null;
}
if(_f.isString(_26)){
_25=_25||_1.global;
if(!_25[_26]){
throw (["lang.hitch: scope[\"",_26,"\"] is null (scope=\"",_25,"\")"].join(""));
}
return function(){
return _25[_26].apply(_25,arguments||[]);
};
}
return !_25?_26:function(){
return _26.apply(_25,arguments||[]);
};
},delegate:(function(){
function TMP(){
};
return function(obj,_27){
TMP.prototype=obj;
var tmp=new TMP();
TMP.prototype=null;
if(_27){
_f._mixin(tmp,_27);
}
return tmp;
};
})(),_toArray:_2("ie")?(function(){
function _28(obj,_29,_2a){
var arr=_2a||[];
for(var x=_29||0;x<obj.length;x++){
arr.push(obj[x]);
}
return arr;
};
return function(obj){
return ((obj.item)?_28:_a).apply(this,arguments);
};
})():_a,partial:function(_2b){
var arr=[null];
return _f.hitch.apply(_1,arr.concat(_f._toArray(arguments)));
},clone:function(src){
if(!src||typeof src!="object"||_f.isFunction(src)){
return src;
}
if(src.nodeType&&"cloneNode" in src){
return src.cloneNode(true);
}
if(src instanceof Date){
return new Date(src.getTime());
}
if(src instanceof RegExp){
return new RegExp(src);
}
var r,i,l;
if(_f.isArray(src)){
r=[];
for(i=0,l=src.length;i<l;++i){
if(i in src){
r[i]=_f.clone(src[i]);
}
}
}else{
r=src.constructor?new src.constructor():{};
}
return _f._mixin(r,src,_f.clone);
},trim:String.prototype.trim?function(str){
return str.trim();
}:function(str){
return str.replace(/^\s\s*/,"").replace(/\s\s*$/,"");
},replace:function(_2c,map,_2d){
return _2c.replace(_2d||_e,_f.isFunction(map)?map:function(_2e,k){
return _f.getObject(k,false,map);
});
}};
1&&_f.mixin(_1,_f);
return _f;
});
