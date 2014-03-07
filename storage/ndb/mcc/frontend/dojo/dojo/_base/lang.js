/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/lang",["./kernel","../has","./sniff"],function(_1,_2){
_2.add("bug-for-in-skips-shadowed",function(){
for(var i in {toString:1}){
return 0;
}
return 1;
});
var _3=_2("bug-for-in-skips-shadowed")?"hasOwnProperty.valueOf.isPrototypeOf.propertyIsEnumerable.toLocaleString.toString.constructor".split("."):[],_4=_3.length,_5=function(_6,_7,_8){
var _9,s,i,_a={};
for(_9 in _7){
s=_7[_9];
if(!(_9 in _6)||(_6[_9]!==s&&(!(_9 in _a)||_a[_9]!==s))){
_6[_9]=_8?_8(s):s;
}
}
if(_2("bug-for-in-skips-shadowed")){
if(_7){
for(i=0;i<_4;++i){
_9=_3[i];
s=_7[_9];
if(!(_9 in _6)||(_6[_9]!==s&&(!(_9 in _a)||_a[_9]!==s))){
_6[_9]=_8?_8(s):s;
}
}
}
}
return _6;
},_b=function(_c,_d){
if(!_c){
_c={};
}
for(var i=1,l=arguments.length;i<l;i++){
_e._mixin(_c,arguments[i]);
}
return _c;
},_f=function(_10,_11,_12){
var p,i=0,_13=_1.global;
if(!_12){
if(!_10.length){
return _13;
}else{
p=_10[i++];
try{
_12=_1.scopeMap[p]&&_1.scopeMap[p][1];
}
catch(e){
}
_12=_12||(p in _13?_13[p]:(_11?_13[p]={}:undefined));
}
}
while(_12&&(p=_10[i++])){
_12=(p in _12?_12[p]:(_11?_12[p]={}:undefined));
}
return _12;
},_14=function(_15,_16,_17){
var _18=_15.split("."),p=_18.pop(),obj=_f(_18,true,_17);
return obj&&p?(obj[p]=_16):undefined;
},_19=function(_1a,_1b,_1c){
return _f(_1a.split("."),_1b,_1c);
},_1d=function(_1e,obj){
return _e.getObject(_1e,false,obj)!==undefined;
},_1f=Object.prototype.toString,_20=function(it){
return (typeof it=="string"||it instanceof String);
},_21=function(it){
return it&&(it instanceof Array||typeof it=="array");
},_22=function(it){
return _1f.call(it)==="[object Function]";
},_23=function(it){
return it!==undefined&&(it===null||typeof it=="object"||_e.isArray(it)||_e.isFunction(it));
},_24=function(it){
return it&&it!==undefined&&!_e.isString(it)&&!_e.isFunction(it)&&!(it.tagName&&it.tagName.toLowerCase()=="form")&&(_e.isArray(it)||isFinite(it.length));
},_25=function(it){
return it&&!_e.isFunction(it)&&/\{\s*\[native code\]\s*\}/.test(String(it));
},_26=function(_27,_28){
for(var i=1,l=arguments.length;i<l;i++){
_e._mixin(_27.prototype,arguments[i]);
}
return _27;
},_29=function(_2a,_2b){
var pre=_2c(arguments,2);
var _2d=_e.isString(_2b);
return function(){
var _2e=_2c(arguments);
var f=_2d?(_2a||_1.global)[_2b]:_2b;
return f&&f.apply(_2a||this,pre.concat(_2e));
};
},_2f=function(_30,_31){
if(arguments.length>2){
return _e._hitchArgs.apply(_1,arguments);
}
if(!_31){
_31=_30;
_30=null;
}
if(_e.isString(_31)){
_30=_30||_1.global;
if(!_30[_31]){
throw (["dojo.hitch: scope[\"",_31,"\"] is null (scope=\"",_30,"\")"].join(""));
}
return function(){
return _30[_31].apply(_30,arguments||[]);
};
}
return !_30?_31:function(){
return _31.apply(_30,arguments||[]);
};
},_32=(function(){
function TMP(){
};
return function(obj,_33){
TMP.prototype=obj;
var tmp=new TMP();
TMP.prototype=null;
if(_33){
_e._mixin(tmp,_33);
}
return tmp;
};
})(),_34=function(obj,_35,_36){
return (_36||[]).concat(Array.prototype.slice.call(obj,_35||0));
},_2c=_2("ie")?(function(){
function _37(obj,_38,_39){
var arr=_39||[];
for(var x=_38||0;x<obj.length;x++){
arr.push(obj[x]);
}
return arr;
};
return function(obj){
return ((obj.item)?_37:_34).apply(this,arguments);
};
})():_34,_3a=function(_3b){
var arr=[null];
return _e.hitch.apply(_1,arr.concat(_e._toArray(arguments)));
},_3c=function(src){
if(!src||typeof src!="object"||_e.isFunction(src)){
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
if(_e.isArray(src)){
r=[];
for(i=0,l=src.length;i<l;++i){
if(i in src){
r.push(_3c(src[i]));
}
}
}else{
r=src.constructor?new src.constructor():{};
}
return _e._mixin(r,src,_3c);
},_3d=String.prototype.trim?function(str){
return str.trim();
}:function(str){
return str.replace(/^\s\s*/,"").replace(/\s\s*$/,"");
},_3e=/\{([^\}]+)\}/g,_3f=function(_40,map,_41){
return _40.replace(_41||_3e,_e.isFunction(map)?map:function(_42,k){
return _19(k,false,map);
});
},_e={_extraNames:_3,_mixin:_5,mixin:_b,setObject:_14,getObject:_19,exists:_1d,isString:_20,isArray:_21,isFunction:_22,isObject:_23,isArrayLike:_24,isAlien:_25,extend:_26,_hitchArgs:_29,hitch:_2f,delegate:_32,_toArray:_2c,partial:_3a,clone:_3c,trim:_3d,replace:_3f};
1&&_b(_1,_e);
return _e;
});
