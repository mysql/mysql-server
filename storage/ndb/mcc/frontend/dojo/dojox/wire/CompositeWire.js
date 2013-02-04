//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/_base,dojox/wire/Wire"],function(_1,_2,_3){
_2.provide("dojox.wire.CompositeWire");
_2.require("dojox.wire._base");
_2.require("dojox.wire.Wire");
_2.declare("dojox.wire.CompositeWire",_3.wire.Wire,{_wireClass:"dojox.wire.CompositeWire",constructor:function(_4){
this._initializeChildren(this.children);
},_getValue:function(_5){
if(!_5||!this.children){
return _5;
}
var _6=(_2.isArray(this.children)?[]:{});
for(var c in this.children){
_6[c]=this.children[c].getValue(_5);
}
return _6;
},_setValue:function(_7,_8){
if(!_7||!this.children){
return _7;
}
for(var c in this.children){
this.children[c].setValue(_8[c],_7);
}
return _7;
},_initializeChildren:function(_9){
if(!_9){
return;
}
for(var c in _9){
var _a=_9[c];
_a.parent=this;
if(!_3.wire.isWire(_a)){
_9[c]=_3.wire.create(_a);
}
}
}});
});
