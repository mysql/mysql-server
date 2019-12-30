//>>built
define("dojox/lang/typed",["dojo","dijit","dojox","dojo/require!dojox/json/schema"],function(_1,_2,_3){
(function(){
var _4,_5=typeof _1!="undefined";
if(_5){
_1.provide("dojox.lang.typed");
_1.require("dojox.json.schema");
_4=_3.json.schema;
}else{
if(typeof JSONSchema=="undefined"){
throw new Error("Dojo or JSON Schema library must be present");
}
_4=JSONSchema;
}
function _6(_7,_8){
var _9=function(){
var _a=_8();
if(_a&&_a.parameters){
var _b=_a.parameters;
for(var j=0;j<_b.length;j++){
arguments[j]=_c(arguments[j],_b[j],j.toString());
}
if(_a.additionalParameters){
for(;j<arguments.length;j++){
arguments[j]=_c(arguments[j],_a.additionalParameters,j.toString());
}
}
}
var _d=_7.apply(this,arguments);
if(_a.returns){
_c(_d,_a.returns);
}
return _d;
};
_9.__typedFunction__=true;
for(var i in _7){
_9[i]=_7[i];
}
return _9;
};
function _e(_f){
return function(){
return _f;
};
};
function _c(_10,_11,_12){
if(typeof _10=="function"&&_11&&!_10.__typedFunction__){
_10=_6(_10,_e(_11));
}
var _13=_4._validate(_10,_11,_12);
if(!_13.valid){
var _14="";
var _15=_13.errors;
for(var i=0;i<_15.length;i++){
_14+=_15[i].property+" "+_15[i].message+"\n";
}
throw new TypeError(_14);
}
return _10;
};
var _16=_4.__defineGetter__;
var _17=function(_18){
if(_18.__typedClass__){
return _18;
}
var _19=function(){
var i,_1a,_1b=_19.properties;
var _1c=_19.methods;
_18.apply(this,arguments);
this.__props__={};
for(i in _1c){
_1a=this[i];
if(_1a){
if(!_1a.__typedFunction__){
var _1d=this;
while(!_1d.hasOwnProperty(i)&&_1d.__proto__){
_1d=_1d.__proto__;
}
(function(i){
_1d[i]=_6(_1a,function(){
return _1c[i];
});
})(i);
}
}else{
(function(i){
this[i]=function(){
throw new TypeError("The method "+i+" is defined but not implemented");
};
})(i);
}
}
if(_16){
var _1e=this;
for(i in _1b){
_1a=this[i];
if(this.hasOwnProperty(i)){
this.__props__[i]=_1a;
}
(function(i){
delete _1e[i];
_1e.__defineGetter__(i,function(){
return i in this.__props__?this.__props__[i]:this.__proto__[i];
});
_1e.__defineSetter__(i,function(_1f){
_c(_1f,_1b[i],i);
return this.__props__[i]=_1f;
});
})(i);
}
}
_c(this,_19);
};
_19.prototype=_18.prototype;
for(var i in _18){
_19[i]=_18[i];
}
if(_18.prototype.declaredClass&&_5){
_1.setObject(_18.prototype.declaredClass,_19);
}
_19.__typedClass__=true;
return _19;
};
if(_5){
_3.lang.typed=_17;
if(_1.config.typeCheckAllClasses){
var _20=_1.declare;
_1.declare=function(_21){
var _22=_20.apply(this,arguments);
_22=_17(_22);
return _22;
};
_1.mixin(_1.declare,_20);
}
}else{
typed=_17;
}
})();
});
