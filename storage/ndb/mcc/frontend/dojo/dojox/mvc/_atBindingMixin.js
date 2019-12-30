//>>built
define("dojox/mvc/_atBindingMixin",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/has","./resolve","./sync"],function(_1,_2,_3,_4,_5,_6){
if(_4("mvc-bindings-log-api")){
function _7(_8,_9){
return [_8._setIdAttr||!_8.declaredClass?_8:_8.declaredClass,_9].join(":");
};
function _a(_b,_c){
console.warn(_c+" could not be resolved"+(typeof _b=="string"?(" with "+_b):"")+".");
};
}
function _d(w){
var _e;
try{
_e=require("dijit/registry");
}
catch(e){
return;
}
var pn=w.domNode&&w.domNode.parentNode,pw,pb;
while(pn){
pw=_e.getEnclosingWidget(pn);
if(pw){
var _f=pw._relTargetProp||"target",pt=_2.isFunction(pw.get)?pw.get(_f):pw[_f];
if(pt||_f in pw.constructor.prototype){
return pw;
}
}
pn=pw&&pw.domNode.parentNode;
}
};
function _10(_11,_12,_13,_14,_15){
var _16={},_17=_d(_13),_18=_17&&_17._relTargetProp||"target";
function _19(){
_16["Two"]&&_16["Two"].unwatch();
delete _16["Two"];
var _1a=_17&&(_2.isFunction(_17.get)?_17.get(_18):_17[_18]),_1b=_5(_11,_1a),_1c=_5(_13,_1a);
if(_4("mvc-bindings-log-api")&&(!_1b||/^rel:/.test(_11)&&!_17)){
_a(_11,_12);
}
if(_4("mvc-bindings-log-api")&&(!_1c||/^rel:/.test(_13)&&!_17)){
_a(_13,_14);
}
if(!_1b||!_1c||(/^rel:/.test(_11)||/^rel:/.test(_13))&&!_17){
return;
}
if((!_1b.set||!_1b.watch)&&_12=="*"){
if(_4("mvc-bindings-log-api")){
_a(_11,_12);
}
return;
}
if(_12==null){
_2.isFunction(_1c.set)?_1c.set(_14,_1b):(_1c[_14]=_1b);
if(_4("mvc-bindings-log-api")){
}
}else{
_16["Two"]=_6(_1b,_12,_1c,_14,_15);
}
};
_19();
if(_17&&/^rel:/.test(_11)||/^rel:/.test(_13)&&_2.isFunction(_17.set)&&_2.isFunction(_17.watch)){
_16["rel"]=_17.watch(_18,function(_1d,old,_1e){
if(old!==_1e){
if(_4("mvc-bindings-log-api")){
}
_19();
}
});
}
var h={};
h.unwatch=h.remove=function(){
for(var s in _16){
_16[s]&&_16[s].unwatch();
delete _16[s];
}
};
return h;
};
var _1f=_3("dojox/mvc/_atBindingMixin",null,{dataBindAttr:"data-mvc-bindings",_dbpostscript:function(_20,_21){
var _22=this._refs=(_20||{}).refs||{};
for(var _23 in _20){
if((_20[_23]||{}).atsignature=="dojox.mvc.at"){
var h=_20[_23];
delete _20[_23];
_22[_23]=h;
}
}
},_startAtWatchHandles:function(){
var _24=this._refs;
if(_24){
var _25=this._atWatchHandles=this._atWatchHandles||{};
this._excludes=null;
for(var _26 in _24){
if(!_24[_26]||_26=="*"){
continue;
}
_25[_26]=_10(_24[_26].target,_24[_26].targetProp,this,_26,{bindDirection:_24[_26].bindDirection,converter:_24[_26].converter});
}
if((_24["*"]||{}).atsignature=="dojox.mvc.at"){
_25["*"]=_10(_24["*"].target,_24["*"].targetProp,this,"*",{bindDirection:_24["*"].bindDirection,converter:_24["*"].converter});
}
}
},_stopAtWatchHandles:function(){
for(var s in this._atWatchHandles){
this._atWatchHandles[s].unwatch();
delete this._atWatchHandles[s];
}
},_setAtWatchHandle:function(_27,_28){
if(_27=="ref"){
throw new Error(this+": 1.7 ref syntax used in conjuction with 1.8 dojox/mvc/at syntax, which is not supported.");
}
var _29=this._atWatchHandles=this._atWatchHandles||{};
if(_29[_27]){
_29[_27].unwatch();
delete _29[_27];
}
this[_27]=null;
this._excludes=null;
if(this._started){
_29[_27]=_10(_28.target,_28.targetProp,this,_27,{bindDirection:_28.bindDirection,converter:_28.converter});
}else{
this._refs[_27]=_28;
}
},_setBind:function(_2a){
var _2b=eval("({"+_2a+"})");
for(var _2c in _2b){
var h=_2b[_2c];
if((h||{}).atsignature!="dojox.mvc.at"){
console.warn(_2c+" in "+dataBindAttr+" is not a data binding handle.");
}else{
this._setAtWatchHandle(_2c,h);
}
}
},_getExcludesAttr:function(){
if(this._excludes){
return this._excludes;
}
var _2d=[];
for(var s in this._atWatchHandles){
if(s!="*"){
_2d.push(s);
}
}
return _2d;
},_getPropertiesAttr:function(){
if(this.constructor._attribs){
return this.constructor._attribs;
}
var _2e=["onClick"].concat(this.constructor._setterAttrs);
_1.forEach(["id","excludes","properties","ref","binding"],function(s){
var _2f=_1.indexOf(_2e,s);
if(_2f>=0){
_2e.splice(_2f,1);
}
});
return this.constructor._attribs=_2e;
}});
_1f.prototype[_1f.prototype.dataBindAttr]="";
return _1f;
});
