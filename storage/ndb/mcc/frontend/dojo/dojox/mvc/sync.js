//>>built
define("dojox/mvc/sync",["dojo/_base/lang","dojo/_base/config","dojo/_base/array","dojo/has"],function(_1,_2,_3,_4){
var _5=_1.getObject("dojox.mvc",true);
_4.add("mvc-bindings-log-api",(_2["mvc"]||{}).debugBindings);
var _6;
if(_4("mvc-bindings-log-api")){
function _7(_8,_9,_a,_b){
return [[_a._setIdAttr||!_a.declaredClass?_a:_a.declaredClass,_b].join(":"),[_8._setIdAttr||!_8.declaredClass?_8:_8.declaredClass,_9].join(":")];
};
}
function _c(_d,_e){
return _d===_e||typeof _d=="number"&&isNaN(_d)&&typeof _e=="number"&&isNaN(_e)||_1.isFunction((_d||{}).getTime)&&_1.isFunction((_e||{}).getTime)&&_d.getTime()==_e.getTime()||(_1.isFunction((_d||{}).equals)?_d.equals(_e):_1.isFunction((_e||{}).equals)?_e.equals(_d):false);
};
function _f(_10,_11,_12,_13,_14,_15,old,_16,_17){
if(_6.equals(_16,old)||_13=="*"&&_3.indexOf(_12.get("properties")||[_15],_15)<0||_13=="*"&&_15 in (_17||{})){
return;
}
var _18=_13=="*"?_15:_13;
if(_4("mvc-bindings-log-api")){
var _19=_7(_12,_18,_14,_15);
}
try{
_16=_10?_10(_16,_11):_16;
}
catch(e){
if(_4("mvc-bindings-log-api")){
}
return;
}
if(_4("mvc-bindings-log-api")){
}
_1.isFunction(_12.set)?_12.set(_18,_16):(_12[_18]=_16);
};
var _1a={from:1,to:2,both:3},_1b;
_6=function(_1c,_1d,_1e,_1f,_20){
var _21=(_20||{}).converter,_22,_23,_24;
if(_21){
_22={source:_1c,target:_1e};
_23=_21.format&&_1.hitch(_22,_21.format);
_24=_21.parse&&_1.hitch(_22,_21.parse);
}
var _25=[],_26=[],_27,_28=_1.mixin({},_1c.constraints,_1e.constraints),_29=(_20||{}).bindDirection||_5.both;
if(_4("mvc-bindings-log-api")){
var _2a=_7(_1c,_1d,_1e,_1f);
}
if(_1f=="*"){
if(_1d!="*"){
throw new Error("Unmatched wildcard is specified between source and target.");
}
_27=_1e.get("properties");
if(!_27){
_27=[];
for(var s in _1e){
if(_1e.hasOwnProperty(s)&&s!="_watchCallbacks"){
_27.push(s);
}
}
}
_26=_1e.get("excludes");
}else{
_27=[_1d];
}
if(_29&_5.from){
if(_1.isFunction(_1c.set)&&_1.isFunction(_1c.watch)){
_25.push(_1c.watch.apply(_1c,((_1d!="*")?[_1d]:[]).concat([function(_2b,old,_2c){
_f(_23,_28,_1e,_1f,_1c,_2b,old,_2c,_26);
}])));
}else{
if(_4("mvc-bindings-log-api")){
}
}
_3.forEach(_27,function(_2d){
if(_1f!="*"||!(_2d in (_26||{}))){
var _2e=_1.isFunction(_1c.get)?_1c.get(_2d):_1c[_2d];
_f(_23,_28,_1e,_1f=="*"?_2d:_1f,_1c,_2d,_1b,_2e);
}
});
}
if(_29&_5.to){
if(!(_29&_5.from)){
_3.forEach(_27,function(_2f){
if(_1f!="*"||!(_2f in (_26||{}))){
var _30=_1.isFunction(_1e.get)?_1e.get(_1f):_1e[_1f];
_f(_24,_28,_1c,_2f,_1e,_1f=="*"?_2f:_1f,_1b,_30);
}
});
}
if(_1.isFunction(_1e.set)&&_1.isFunction(_1e.watch)){
_25.push(_1e.watch.apply(_1e,((_1f!="*")?[_1f]:[]).concat([function(_31,old,_32){
_f(_24,_28,_1c,_1d,_1e,_31,old,_32,_26);
}])));
}else{
if(_4("mvc-bindings-log-api")){
}
}
}
if(_4("mvc-bindings-log-api")){
}
var _33={};
_33.unwatch=_33.remove=function(){
for(var h=null;h=_25.pop();){
h.unwatch();
if(_4("mvc-bindings-log-api")){
}
}
};
return _33;
};
_1.mixin(_5,_1a);
return _1.setObject("dojox.mvc.sync",_1.mixin(_6,{equals:_c},_1a));
});
