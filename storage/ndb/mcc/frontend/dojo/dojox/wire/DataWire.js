//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/Wire"],function(_1,_2,_3){
_2.provide("dojox.wire.DataWire");
_2.require("dojox.wire.Wire");
_2.declare("dojox.wire.DataWire",_3.wire.Wire,{_wireClass:"dojox.wire.DataWire",constructor:function(_4){
if(!this.dataStore&&this.parent){
this.dataStore=this.parent.dataStore;
}
},_getValue:function(_5){
if(!_5||!this.attribute||!this.dataStore){
return _5;
}
var _6=_5;
var _7=this.attribute.split(".");
for(var i in _7){
_6=this._getAttributeValue(_6,_7[i]);
if(!_6){
return undefined;
}
}
return _6;
},_setValue:function(_8,_9){
if(!_8||!this.attribute||!this.dataStore){
return _8;
}
var _a=_8;
var _b=this.attribute.split(".");
var _c=_b.length-1;
for(var i=0;i<_c;i++){
_a=this._getAttributeValue(_a,_b[i]);
if(!_a){
return undefined;
}
}
this._setAttributeValue(_a,_b[_c],_9);
return _8;
},_getAttributeValue:function(_d,_e){
var _f=undefined;
var i1=_e.indexOf("[");
if(i1>=0){
var i2=_e.indexOf("]");
var _10=_e.substring(i1+1,i2);
_e=_e.substring(0,i1);
var _11=this.dataStore.getValues(_d,_e);
if(_11){
if(!_10){
_f=_11;
}else{
_f=_11[_10];
}
}
}else{
_f=this.dataStore.getValue(_d,_e);
}
return _f;
},_setAttributeValue:function(_12,_13,_14){
var i1=_13.indexOf("[");
if(i1>=0){
var i2=_13.indexOf("]");
var _15=_13.substring(i1+1,i2);
_13=_13.substring(0,i1);
var _16=null;
if(!_15){
_16=_14;
}else{
_16=this.dataStore.getValues(_12,_13);
if(!_16){
_16=[];
}
_16[_15]=_14;
}
this.dataStore.setValues(_12,_13,_16);
}else{
this.dataStore.setValue(_12,_13,_14);
}
}});
});
