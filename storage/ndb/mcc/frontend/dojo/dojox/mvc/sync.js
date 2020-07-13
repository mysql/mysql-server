//>>built
define("dojox/mvc/sync",["dojo/_base/lang","dojo/_base/config","dojo/_base/array","dojo/has"],function(_1,_2,_3,_4){
var _5=_1.getObject("dojox.mvc",true);
_4.add("mvc-bindings-log-api",(_2["mvc"]||{}).debugBindings);
var _6;
var _7=(_4("mvc-bindings-log-api"))?function(_8,_9,_a,_b){
return [[_a.canConvertToLoggable||!_a.declaredClass?_a:_a.declaredClass,_b].join(":"),[_8.canConvertToLoggable||!_8.declaredClass?_8:_8.declaredClass,_9].join(":")];
}:"";
function _c(_d,_e){
return _d===_e||typeof _d=="number"&&isNaN(_d)&&typeof _e=="number"&&isNaN(_e)||_1.isFunction((_d||{}).getTime)&&_1.isFunction((_e||{}).getTime)&&_d.getTime()==_e.getTime()||(_1.isFunction((_d||{}).equals)?_d.equals(_e):_1.isFunction((_e||{}).equals)?_e.equals(_d):false);
};
function _f(_10,_11,_12,_13,_14,_15,_16,old,_17,_18){
if(_12(_17,old)||_14=="*"&&_3.indexOf(_13.get("properties")||[_16],_16)<0||_14=="*"&&_16 in (_18||{})){
return;
}
var _19=_14=="*"?_16:_14;
if(_4("mvc-bindings-log-api")){
var _1a=_7(_13,_19,_15,_16);
}
try{
_17=_10?_10(_17,_11):_17;
}
catch(e){
if(_4("mvc-bindings-log-api")){
}
return;
}
if(_4("mvc-bindings-log-api")){
}
_1.isFunction(_13.set)?_13.set(_19,_17):(_13[_19]=_17);
};
var _1b={from:1,to:2,both:3},_1c;
_6=function(_1d,_1e,_1f,_20,_21){
var _22=(_21||{}).converter,_23,_24,_25;
if(_22){
_23={source:_1d,target:_1f};
_24=_22.format&&_1.hitch(_23,_22.format);
_25=_22.parse&&_1.hitch(_23,_22.parse);
}
var _26=[],_27=[],_28,_29=_1.mixin({},_1d.constraints,_1f.constraints),_2a=(_21||{}).bindDirection||_5.both,_c=(_21||{}).equals||_6.equals;
if(_4("mvc-bindings-log-api")){
var _2b=_7(_1d,_1e,_1f,_20);
}
if(_20=="*"){
if(_1e!="*"){
throw new Error("Unmatched wildcard is specified between source and target.");
}
_28=_1f.get("properties");
if(!_28){
_28=[];
for(var s in _1f){
if(_1f.hasOwnProperty(s)&&s!="_watchCallbacks"){
_28.push(s);
}
}
}
_27=_1f.get("excludes");
}else{
_28=[_1e];
}
if(_2a&_5.from){
if(_1.isFunction(_1d.set)&&_1.isFunction(_1d.watch)){
_26.push(_1d.watch.apply(_1d,((_1e!="*")?[_1e]:[]).concat([function(_2c,old,_2d){
_f(_24,_29,_c,_1f,_20,_1d,_2c,old,_2d,_27);
}])));
}else{
if(_4("mvc-bindings-log-api")){
}
}
_3.forEach(_28,function(_2e){
if(_20!="*"||!(_2e in (_27||{}))){
var _2f=_1.isFunction(_1d.get)?_1d.get(_2e):_1d[_2e];
_f(_24,_29,_c,_1f,_20=="*"?_2e:_20,_1d,_2e,_1c,_2f);
}
});
}
if(_2a&_5.to){
if(!(_2a&_5.from)){
_3.forEach(_28,function(_30){
if(_20!="*"||!(_30 in (_27||{}))){
var _31=_1.isFunction(_1f.get)?_1f.get(_20):_1f[_20];
_f(_25,_29,_c,_1d,_30,_1f,_20=="*"?_30:_20,_1c,_31);
}
});
}
if(_1.isFunction(_1f.set)&&_1.isFunction(_1f.watch)){
_26.push(_1f.watch.apply(_1f,((_20!="*")?[_20]:[]).concat([function(_32,old,_33){
_f(_25,_29,_c,_1d,_1e,_1f,_32,old,_33,_27);
}])));
}else{
if(_4("mvc-bindings-log-api")){
}
}
}
if(_4("mvc-bindings-log-api")){
}
var _34={};
_34.unwatch=_34.remove=function(){
for(var h=null;h=_26.pop();){
h.unwatch();
}
if(_4("mvc-bindings-log-api")){
}
};
return _34;
};
_1.mixin(_5,_1b);
return _1.setObject("dojox.mvc.sync",_1.mixin(_6,{equals:_c},_1b));
});
