//>>built
define("dojox/mvc/computed",["dojo/_base/array","dojo/_base/lang","dojo/has"],function(_1,_2,_3){
"use strict";
_3.add("object-is-api",_2.isFunction(Object.is));
var _4=Array.prototype,_5=_3("object-is-api")?Object.is:function(_6,_7){
return _6===_7&&(_6!==0||1/_6===1/_7)||_6!==_6&&_7!==_7;
};
function _8(o,_9,_a){
var _b;
if(o&&_2.isFunction(o.watch)){
_b=o.watch(_9,function(_c,_d,_e){
if(!_5(_d,_e)){
_a(_e,_d);
}
});
}else{
}
return {remove:function(){
if(_b){
_b.remove();
}
}};
};
function _f(_10){
return _1.map(_10,function(p){
return p.each?_1.map(p.target,function(_11){
return _11.get?_11.get(p.targetProp):_11[p.targetProp];
}):p.target.get?p.target.get(p.targetProp):p.target[p.targetProp];
});
};
function _12(_13){
for(var h=null;(h=_13.shift());){
h.remove();
}
};
return function(_14,_15,_16){
function _17(_18){
var _19,_1a;
try{
_19=_16.apply(_14,_18);
_1a=true;
}
catch(e){
console.error("Error during computed property callback: "+(e&&e.stack||e));
}
if(_1a){
if(_2.isFunction(_14.set)){
_14.set(_15,_19);
}else{
_14[_15]=_19;
}
}
};
if(_14==null){
throw new Error("Computed property cannot be applied to null.");
}
if(_15==="*"){
throw new Error("Wildcard property cannot be used for computed properties.");
}
var _1b=_4.slice.call(arguments,3),_1c=_1.map(_1b,function(dep,_1d){
function _1e(_1f){
return _8(_1f,dep.targetProp,function(){
_17(_f(_1b));
});
};
if(dep.targetProp==="*"){
throw new Error("Wildcard property cannot be used for computed properties.");
}else{
if(dep.each){
var _20,_21=_1.map(dep.target,_1e);
if(dep.target&&_2.isFunction(dep.target.watchElements)){
_20=dep.target.watchElements(function(idx,_22,_23){
_12(_4.splice.apply(_21,[idx,_22.length].concat(_1.map(_23,_1e))));
_17(_f(_1b));
});
}else{
}
return {remove:function(){
if(_20){
_20.remove();
}
_12(_21);
}};
}else{
return _8(dep.target,dep.targetProp,function(_24){
var _25=[];
_4.push.apply(_25,_f(_1b.slice(0,_1d)));
_25.push(_24);
_4.push.apply(_25,_f(_1b.slice(_1d+1)));
_17(_25);
});
}
}
});
_17(_f(_1b));
return {remove:function(){
_12(_1c);
}};
};
});
