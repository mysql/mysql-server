//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Container,dojox/wire/_base,dojox/wire/ml/Action"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.Transfer");
_2.require("dijit._Widget");
_2.require("dijit._Container");
_2.require("dojox.wire._base");
_2.require("dojox.wire.ml.Action");
_2.declare("dojox.wire.ml.Transfer",_3.wire.ml.Action,{source:"",sourceStore:"",sourceAttribute:"",sourcePath:"",type:"",converter:"",delimiter:"",target:"",targetStore:"",targetAttribute:"",targetPath:"",_run:function(){
var _4=this._getWire("source");
var _5=this._getWire("target");
_3.wire.transfer(_4,_5,arguments);
},_getWire:function(_6){
var _7=undefined;
if(_6=="source"){
_7={object:this.source,dataStore:this.sourceStore,attribute:this.sourceAttribute,path:this.sourcePath,type:this.type,converter:this.converter};
}else{
_7={object:this.target,dataStore:this.targetStore,attribute:this.targetAttribute,path:this.targetPath};
}
if(_7.object){
if(_7.object.length>=9&&_7.object.substring(0,9)=="arguments"){
_7.property=_7.object.substring(9);
_7.object=null;
}else{
var i=_7.object.indexOf(".");
if(i<0){
_7.object=_3.wire.ml._getValue(_7.object);
}else{
_7.property=_7.object.substring(i+1);
_7.object=_3.wire.ml._getValue(_7.object.substring(0,i));
}
}
}
if(_7.dataStore){
_7.dataStore=_3.wire.ml._getValue(_7.dataStore);
}
var _8=undefined;
var _9=this.getChildren();
for(var i in _9){
var _a=_9[i];
if(_a instanceof _3.wire.ml.ChildWire&&_a.which==_6){
if(!_8){
_8={};
}
_a._addWire(this,_8);
}
}
if(_8){
_8.object=_3.wire.create(_7);
_8.dataStore=_7.dataStore;
_7=_8;
}
return _7;
}});
_2.declare("dojox.wire.ml.ChildWire",_1._Widget,{which:"source",object:"",property:"",type:"",converter:"",attribute:"",path:"",name:"",_addWire:function(_b,_c){
if(this.name){
if(!_c.children){
_c.children={};
}
_c.children[this.name]=this._getWire(_b);
}else{
if(!_c.children){
_c.children=[];
}
_c.children.push(this._getWire(_b));
}
},_getWire:function(_d){
return {object:(this.object?_3.wire.ml._getValue(this.object):undefined),property:this.property,type:this.type,converter:this.converter,attribute:this.attribute,path:this.path};
}});
_2.declare("dojox.wire.ml.ColumnWire",_3.wire.ml.ChildWire,{column:"",_addWire:function(_e,_f){
if(this.column){
if(!_f.columns){
_f.columns={};
}
_f.columns[this.column]=this._getWire(_e);
}else{
if(!_f.columns){
_f.columns=[];
}
_f.columns.push(this._getWire(_e));
}
}});
_2.declare("dojox.wire.ml.NodeWire",[_3.wire.ml.ChildWire,_1._Container],{titleProperty:"",titleAttribute:"",titlePath:"",_addWire:function(_10,_11){
if(!_11.nodes){
_11.nodes=[];
}
_11.nodes.push(this._getWires(_10));
},_getWires:function(_12){
var _13={node:this._getWire(_12),title:{type:"string",property:this.titleProperty,attribute:this.titleAttribute,path:this.titlePath}};
var _14=[];
var _15=this.getChildren();
for(var i in _15){
var _16=_15[i];
if(_16 instanceof _3.wire.ml.NodeWire){
_14.push(_16._getWires(_12));
}
}
if(_14.length>0){
_13.children=_14;
}
return _13;
}});
_2.declare("dojox.wire.ml.SegmentWire",_3.wire.ml.ChildWire,{_addWire:function(_17,_18){
if(!_18.segments){
_18.segments=[];
}
_18.segments.push(this._getWire(_17));
if(_17.delimiter&&!_18.delimiter){
_18.delimiter=_17.delimiter;
}
}});
});
