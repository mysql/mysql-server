/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/request/util",["exports","../errors/RequestError","../errors/CancelError","../Deferred","../io-query","../_base/array","../_base/lang","../promise/Promise","../has"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
function _a(_b){
return _9("native-arraybuffer")&&_b instanceof ArrayBuffer;
};
function _c(_d){
return _9("native-blob")&&_d instanceof Blob;
};
function _e(_f){
if(typeof Element!=="undefined"){
return _f instanceof Element;
}
return _f.nodeType===1;
};
function _10(_11){
return _9("native-formdata")&&_11 instanceof FormData;
};
function _12(_13){
return _13&&typeof _13==="object"&&!_10(_13)&&!_e(_13)&&!_c(_13)&&!_a(_13);
};
_1.deepCopy=function(_14,_15){
for(var _16 in _15){
var _17=_14[_16],_18=_15[_16];
if(_16!=="__proto__"&&_17!==_18){
if(_12(_18)){
if(Object.prototype.toString.call(_18)==="[object Date]"){
_14[_16]=new Date(_18);
}else{
if(_7.isArray(_18)){
_14[_16]=_1.deepCopyArray(_18);
}else{
if(_17&&typeof _17==="object"){
_1.deepCopy(_17,_18);
}else{
_14[_16]=_1.deepCopy({},_18);
}
}
}
}else{
_14[_16]=_18;
}
}
}
return _14;
};
_1.deepCopyArray=function(_19){
var _1a=[];
for(var i=0,l=_19.length;i<l;i++){
var _1b=_19[i];
if(typeof _1b==="object"){
_1a.push(_1.deepCopy({},_1b));
}else{
_1a.push(_1b);
}
}
return _1a;
};
_1.deepCreate=function deepCreate(_1c,_1d){
_1d=_1d||{};
var _1e=_7.delegate(_1c),_1f,_20;
for(_1f in _1c){
_20=_1c[_1f];
if(_20&&typeof _20==="object"){
_1e[_1f]=_1.deepCreate(_20,_1d[_1f]);
}
}
return _1.deepCopy(_1e,_1d);
};
var _21=Object.freeze||function(obj){
return obj;
};
function _22(_23){
return _21(_23);
};
function _24(_25){
return _25.data!==undefined?_25.data:_25.text;
};
_1.deferred=function deferred(_26,_27,_28,_29,_2a,_2b){
var def=new _4(function(_2c){
_27&&_27(def,_26);
if(!_2c||!(_2c instanceof _2)&&!(_2c instanceof _3)){
return new _3("Request canceled",_26);
}
return _2c;
});
def.response=_26;
def.isValid=_28;
def.isReady=_29;
def.handleResponse=_2a;
function _2d(_2e){
_2e.response=_26;
throw _2e;
};
var _2f=def.then(_22).otherwise(_2d);
if(_1.notify){
_2f.then(_7.hitch(_1.notify,"emit","load"),_7.hitch(_1.notify,"emit","error"));
}
var _30=_2f.then(_24);
var _31=new _8();
for(var _32 in _30){
if(_30.hasOwnProperty(_32)){
_31[_32]=_30[_32];
}
}
_31.response=_2f;
_21(_31);
if(_2b){
def.then(function(_33){
_2b.call(def,_33);
},function(_34){
_2b.call(def,_26,_34);
});
}
def.promise=_31;
def.then=_31.then;
return def;
};
_1.addCommonMethods=function addCommonMethods(_35,_36){
_6.forEach(_36||["GET","POST","PUT","DELETE"],function(_37){
_35[(_37==="DELETE"?"DEL":_37).toLowerCase()]=function(url,_38){
_38=_7.delegate(_38||{});
_38.method=_37;
return _35(url,_38);
};
});
};
_1.parseArgs=function parseArgs(url,_39,_3a){
var _3b=_39.data,_3c=_39.query;
if(_3b&&!_3a){
if(typeof _3b==="object"&&(!(_9("native-xhr2"))||!(_a(_3b)||_c(_3b)))){
_39.data=_5.objectToQuery(_3b);
}
}
if(_3c){
if(typeof _3c==="object"){
_3c=_5.objectToQuery(_3c);
}
if(_39.preventCache){
_3c+=(_3c?"&":"")+"request.preventCache="+(+(new Date));
}
}else{
if(_39.preventCache){
_3c="request.preventCache="+(+(new Date));
}
}
if(url&&_3c){
url+=(~url.indexOf("?")?"&":"?")+_3c;
}
return {url:url,options:_39,getHeader:function(_3d){
return null;
}};
};
_1.checkStatus=function(_3e){
_3e=_3e||0;
return (_3e>=200&&_3e<300)||_3e===304||_3e===1223||!_3e;
};
});
