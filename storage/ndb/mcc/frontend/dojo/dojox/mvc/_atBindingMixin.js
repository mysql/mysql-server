//>>built
define("dojox/mvc/_atBindingMixin",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/has","dojo/Stateful","./resolve","./sync"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=(_4("mvc-bindings-log-api"))?function(_9,_a){
return [_9._setIdAttr||!_9.declaredClass?_9:_9.declaredClass,_a].join(":");
}:"";
var _b=(_4("mvc-bindings-log-api"))?function(_c,_d){
console.warn(_d+" could not be resolved"+(typeof _c=="string"?(" with "+_c):"")+".");
}:"";
function _e(w){
var _f;
try{
_f=require("dijit/registry");
}
catch(e){
return;
}
var pn=w.domNode&&w.domNode.parentNode,pw,pb;
while(pn){
pw=_f.getEnclosingWidget(pn);
if(pw){
var _10=pw._relTargetProp||"target",pt=_2.isFunction(pw.get)?pw.get(_10):pw[_10];
if(pt||_10 in pw.constructor.prototype){
return pw;
}
}
pn=pw&&pw.domNode.parentNode;
}
};
function _11(_12,_13,_14,_15,_16){
var _17={},_18=_e(_14),_19=_18&&_18._relTargetProp||"target";
function _1a(){
_17["Two"]&&_17["Two"].unwatch();
delete _17["Two"];
var _1b=_18&&(_2.isFunction(_18.get)?_18.get(_19):_18[_19]),_1c=_6(_12,_1b),_1d=_6(_14,_1b);
if(_4("mvc-bindings-log-api")&&(!_1c||/^rel:/.test(_12)&&!_18)){
_b(_12,_13);
}
if(_4("mvc-bindings-log-api")&&(!_1d||/^rel:/.test(_14)&&!_18)){
_b(_14,_15);
}
if(!_1c||!_1d||(/^rel:/.test(_12)||/^rel:/.test(_14))&&!_18){
return;
}
if((!_1c.set||!_1c.watch)&&_13=="*"){
if(_4("mvc-bindings-log-api")){
_b(_12,_13);
}
return;
}
if(_13==null){
_2.isFunction(_1d.set)?_1d.set(_15,_1c):(_1d[_15]=_1c);
if(_4("mvc-bindings-log-api")){
}
}else{
_17["Two"]=_7(_1c,_13,_1d,_15,_16);
}
};
_1a();
if(_18&&/^rel:/.test(_12)||/^rel:/.test(_14)&&_2.isFunction(_18.set)&&_2.isFunction(_18.watch)){
_17["rel"]=_18.watch(_19,function(_1e,old,_1f){
if(old!==_1f){
if(_4("mvc-bindings-log-api")){
}
_1a();
}
});
}
var h={};
h.unwatch=h.remove=function(){
for(var s in _17){
_17[s]&&_17[s].unwatch();
delete _17[s];
}
};
return h;
};
var _20={dataBindAttr:"data-mvc-bindings",_dbpostscript:function(_21,_22){
var _23=this._refs=(_21||{}).refs||{};
for(var _24 in _21){
if((_21[_24]||{}).atsignature=="dojox.mvc.at"){
var h=_21[_24];
delete _21[_24];
_23[_24]=h;
}
}
var _25=new _5(),_26=this;
_25.toString=function(){
return "[Mixin value of widget "+_26.declaredClass+", "+(_26.id||"NO ID")+"]";
};
_25.canConvertToLoggable=true;
this._startAtWatchHandles(_25);
for(var _24 in _23){
if(_25[_24]!==void 0){
(_21=_21||{})[_24]=_25[_24];
}
}
this._stopAtWatchHandles();
},_startAtWatchHandles:function(_27){
this.canConvertToLoggable=true;
var _28=this._refs;
if(_28){
var _29=this._atWatchHandles=this._atWatchHandles||{};
this._excludes=null;
for(var _2a in _28){
if(!_28[_2a]||_2a=="*"){
continue;
}
_29[_2a]=_11(_28[_2a].target,_28[_2a].targetProp,_27||this,_2a,{bindDirection:_28[_2a].bindDirection,converter:_28[_2a].converter,equals:_28[_2a].equalsCallback});
}
if((_28["*"]||{}).atsignature=="dojox.mvc.at"){
_29["*"]=_11(_28["*"].target,_28["*"].targetProp,_27||this,"*",{bindDirection:_28["*"].bindDirection,converter:_28["*"].converter,equals:_28["*"].equalsCallback});
}
}
},_stopAtWatchHandles:function(){
for(var s in this._atWatchHandles){
this._atWatchHandles[s].unwatch();
delete this._atWatchHandles[s];
}
},_setAtWatchHandle:function(_2b,_2c){
if(_2b=="ref"){
throw new Error(this+": 1.7 ref syntax used in conjunction with 1.8 dojox/mvc/at syntax, which is not supported.");
}
var _2d=this._atWatchHandles=this._atWatchHandles||{};
if(_2d[_2b]){
_2d[_2b].unwatch();
delete _2d[_2b];
}
this[_2b]=null;
this._excludes=null;
if(this._started){
_2d[_2b]=_11(_2c.target,_2c.targetProp,this,_2b,{bindDirection:_2c.bindDirection,converter:_2c.converter,equals:_2c.equalsCallback});
}else{
this._refs[_2b]=_2c;
}
},_setBind:function(_2e){
var _2f=eval("({"+_2e+"})");
for(var _30 in _2f){
var h=_2f[_30];
if((h||{}).atsignature!="dojox.mvc.at"){
console.warn(_30+" in "+dataBindAttr+" is not a data binding handle.");
}else{
this._setAtWatchHandle(_30,h);
}
}
},_getExcludesAttr:function(){
if(this._excludes){
return this._excludes;
}
var _31=[];
for(var s in this._atWatchHandles){
if(s!="*"){
_31.push(s);
}
}
return _31;
},_getPropertiesAttr:function(){
if(this.constructor._attribs){
return this.constructor._attribs;
}
var _32=["onClick"].concat(this.constructor._setterAttrs);
_1.forEach(["id","excludes","properties","ref","binding"],function(s){
var _33=_1.indexOf(_32,s);
if(_33>=0){
_32.splice(_33,1);
}
});
return this.constructor._attribs=_32;
}};
_20[_20.dataBindAttr]="";
var _34=_3("dojox/mvc/_atBindingMixin",null,_20);
_34.mixin=_20;
return _34;
});
