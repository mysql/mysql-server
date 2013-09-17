//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/CompositeWire"],function(_1,_2,_3){
_2.provide("dojox.wire.TreeAdapter");
_2.require("dojox.wire.CompositeWire");
_2.declare("dojox.wire.TreeAdapter",_3.wire.CompositeWire,{_wireClass:"dojox.wire.TreeAdapter",constructor:function(_4){
this._initializeChildren(this.nodes);
},_getValue:function(_5){
if(!_5||!this.nodes){
return _5;
}
var _6=_5;
if(!_2.isArray(_6)){
_6=[_6];
}
var _7=[];
for(var i in _6){
for(var i2 in this.nodes){
_7=_7.concat(this._getNodes(_6[i],this.nodes[i2]));
}
}
return _7;
},_setValue:function(_8,_9){
throw new Error("Unsupported API: "+this._wireClass+"._setValue");
},_initializeChildren:function(_a){
if(!_a){
return;
}
for(var i in _a){
var _b=_a[i];
if(_b.node){
_b.node.parent=this;
if(!_3.wire.isWire(_b.node)){
_b.node=_3.wire.create(_b.node);
}
}
if(_b.title){
_b.title.parent=this;
if(!_3.wire.isWire(_b.title)){
_b.title=_3.wire.create(_b.title);
}
}
if(_b.children){
this._initializeChildren(_b.children);
}
}
},_getNodes:function(_c,_d){
var _e=null;
if(_d.node){
_e=_d.node.getValue(_c);
if(!_e){
return [];
}
if(!_2.isArray(_e)){
_e=[_e];
}
}else{
_e=[_c];
}
var _f=[];
for(var i in _e){
_c=_e[i];
var _10={};
if(_d.title){
_10.title=_d.title.getValue(_c);
}else{
_10.title=_c;
}
if(_d.children){
var _11=[];
for(var i2 in _d.children){
_11=_11.concat(this._getNodes(_c,_d.children[i2]));
}
if(_11.length>0){
_10.children=_11;
}
}
_f.push(_10);
}
return _f;
}});
});
