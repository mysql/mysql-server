//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Container,dojox/wire/ml/util"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.Data");
_2.require("dijit._Widget");
_2.require("dijit._Container");
_2.require("dojox.wire.ml.util");
_2.declare("dojox.wire.ml.Data",[_1._Widget,_1._Container],{startup:function(){
this._initializeProperties();
},_initializeProperties:function(_4){
if(!this._properties||_4){
this._properties={};
}
var _5=this.getChildren();
for(var i in _5){
var _6=_5[i];
if((_6 instanceof _3.wire.ml.DataProperty)&&_6.name){
this.setPropertyValue(_6.name,_6.getValue());
}
}
},getPropertyValue:function(_7){
return this._properties[_7];
},setPropertyValue:function(_8,_9){
this._properties[_8]=_9;
}});
_2.declare("dojox.wire.ml.DataProperty",[_1._Widget,_1._Container],{name:"",type:"",value:"",_getValueAttr:function(){
return this.getValue();
},getValue:function(){
var _a=this.value;
if(this.type){
if(this.type=="number"){
_a=parseInt(_a);
}else{
if(this.type=="boolean"){
_a=(_a=="true");
}else{
if(this.type=="array"){
_a=[];
var _b=this.getChildren();
for(var i in _b){
var _c=_b[i];
if(_c instanceof _3.wire.ml.DataProperty){
_a.push(_c.getValue());
}
}
}else{
if(this.type=="object"){
_a={};
var _b=this.getChildren();
for(var i in _b){
var _c=_b[i];
if((_c instanceof _3.wire.ml.DataProperty)&&_c.name){
_a[_c.name]=_c.getValue();
}
}
}else{
if(this.type=="element"){
_a=new _3.wire.ml.XmlElement(_a);
var _b=this.getChildren();
for(var i in _b){
var _c=_b[i];
if((_c instanceof _3.wire.ml.DataProperty)&&_c.name){
_a.setPropertyValue(_c.name,_c.getValue());
}
}
}
}
}
}
}
}
return _a;
}});
});
