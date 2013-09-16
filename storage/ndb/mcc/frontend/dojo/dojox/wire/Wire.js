//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/_base"],function(_1,_2,_3){
_2.provide("dojox.wire.Wire");
_2.require("dojox.wire._base");
_2.declare("dojox.wire.Wire",null,{_wireClass:"dojox.wire.Wire",constructor:function(_4){
_2.mixin(this,_4);
if(this.converter){
if(_2.isString(this.converter)){
var _5=_2.getObject(this.converter);
if(_2.isFunction(_5)){
try{
var _6=new _5();
if(_6&&!_2.isFunction(_6["convert"])){
this.converter={convert:_5};
}else{
this.converter=_6;
}
}
catch(e){
}
}else{
if(_2.isObject(_5)){
if(_2.isFunction(_5["convert"])){
this.converter=_5;
}
}
}
if(_2.isString(this.converter)){
var _7=_3.wire._getClass(this.converter);
if(_7){
this.converter=new _7();
}else{
this.converter=undefined;
}
}
}else{
if(_2.isFunction(this.converter)){
this.converter={convert:this.converter};
}
}
}
},getValue:function(_8){
var _9=undefined;
if(_3.wire.isWire(this.object)){
_9=this.object.getValue(_8);
}else{
_9=(this.object||_8);
}
if(this.property){
var _a=this.property.split(".");
for(var i in _a){
if(!_9){
return _9;
}
_9=this._getPropertyValue(_9,_a[i]);
}
}
var _b=undefined;
if(this._getValue){
_b=this._getValue(_9);
}else{
_b=_9;
}
if(_b){
if(this.type){
if(this.type=="string"){
_b=_b.toString();
}else{
if(this.type=="number"){
_b=parseInt(_b,10);
}else{
if(this.type=="boolean"){
_b=(_b!="false");
}else{
if(this.type=="array"){
if(!_2.isArray(_b)){
_b=[_b];
}
}
}
}
}
}
if(this.converter&&this.converter.convert){
_b=this.converter.convert(_b,this);
}
}
return _b;
},setValue:function(_c,_d){
var _e=undefined;
if(_3.wire.isWire(this.object)){
_e=this.object.getValue(_d);
}else{
_e=(this.object||_d);
}
var _f=undefined;
var o;
if(this.property){
if(!_e){
if(_3.wire.isWire(this.object)){
_e={};
this.object.setValue(_e,_d);
}else{
throw new Error(this._wireClass+".setValue(): invalid object");
}
}
var _10=this.property.split(".");
var _11=_10.length-1;
for(var i=0;i<_11;i++){
var p=_10[i];
o=this._getPropertyValue(_e,p);
if(!o){
o={};
this._setPropertyValue(_e,p,o);
}
_e=o;
}
_f=_10[_11];
}
if(this._setValue){
if(_f){
o=this._getPropertyValue(_e,_f);
if(!o){
o={};
this._setPropertyValue(_e,_f,o);
}
_e=o;
}
var _12=this._setValue(_e,_c);
if(!_e&&_12){
if(_3.wire.isWire(this.object)){
this.object.setValue(_12,_d);
}else{
throw new Error(this._wireClass+".setValue(): invalid object");
}
}
}else{
if(_f){
this._setPropertyValue(_e,_f,_c);
}else{
if(_3.wire.isWire(this.object)){
this.object.setValue(_c,_d);
}else{
throw new Error(this._wireClass+".setValue(): invalid property");
}
}
}
},_getPropertyValue:function(_13,_14){
var _15=undefined;
var i1=_14.indexOf("[");
if(i1>=0){
var i2=_14.indexOf("]");
var _16=_14.substring(i1+1,i2);
var _17=null;
if(i1===0){
_17=_13;
}else{
_14=_14.substring(0,i1);
_17=this._getPropertyValue(_13,_14);
if(_17&&!_2.isArray(_17)){
_17=[_17];
}
}
if(_17){
_15=_17[_16];
}
}else{
if(_13.getPropertyValue){
_15=_13.getPropertyValue(_14);
}else{
var _18="get"+_14.charAt(0).toUpperCase()+_14.substring(1);
if(this._useGet(_13)){
_15=_13.get(_14);
}else{
if(this._useAttr(_13)){
_15=_13.attr(_14);
}else{
if(_13[_18]){
_15=_13[_18]();
}else{
_15=_13[_14];
}
}
}
}
}
return _15;
},_setPropertyValue:function(_19,_1a,_1b){
var i1=_1a.indexOf("[");
if(i1>=0){
var i2=_1a.indexOf("]");
var _1c=_1a.substring(i1+1,i2);
var _1d=null;
if(i1===0){
_1d=_19;
}else{
_1a=_1a.substring(0,i1);
_1d=this._getPropertyValue(_19,_1a);
if(!_1d){
_1d=[];
this._setPropertyValue(_19,_1a,_1d);
}
}
_1d[_1c]=_1b;
}else{
if(_19.setPropertyValue){
_19.setPropertyValue(_1a,_1b);
}else{
var _1e="set"+_1a.charAt(0).toUpperCase()+_1a.substring(1);
if(this._useSet(_19)){
_19.set(_1a,_1b);
}else{
if(this._useAttr(_19)){
_19.attr(_1a,_1b);
}else{
if(_19[_1e]){
_19[_1e](_1b);
}else{
_19[_1a]=_1b;
}
}
}
}
}
},_useGet:function(_1f){
var _20=false;
if(_2.isFunction(_1f.get)){
_20=true;
}
return _20;
},_useSet:function(_21){
var _22=false;
if(_2.isFunction(_21.set)){
_22=true;
}
return _22;
},_useAttr:function(_23){
var _24=false;
if(_2.isFunction(_23.attr)){
_24=true;
}
return _24;
}});
});
