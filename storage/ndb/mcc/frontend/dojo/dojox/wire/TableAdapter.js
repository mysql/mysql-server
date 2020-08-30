//>>built
define("dojox/wire/TableAdapter",["dojo","dijit","dojox","dojo/require!dojox/wire/CompositeWire"],function(_1,_2,_3){
_1.provide("dojox.wire.TableAdapter");
_1.require("dojox.wire.CompositeWire");
_1.declare("dojox.wire.TableAdapter",_3.wire.CompositeWire,{_wireClass:"dojox.wire.TableAdapter",constructor:function(_4){
this._initializeChildren(this.columns);
},_getValue:function(_5){
if(!_5||!this.columns){
return _5;
}
var _6=_5;
if(!_1.isArray(_6)){
_6=[_6];
}
var _7=[];
for(var i in _6){
var _8=this._getRow(_6[i]);
_7.push(_8);
}
return _7;
},_setValue:function(_9,_a){
throw new Error("Unsupported API: "+this._wireClass+"._setValue");
},_getRow:function(_b){
var _c=(_1.isArray(this.columns)?[]:{});
for(var c in this.columns){
_c[c]=this.columns[c].getValue(_b);
}
return _c;
}});
});
