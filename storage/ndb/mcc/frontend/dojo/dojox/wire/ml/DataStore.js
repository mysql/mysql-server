//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dojox/wire/_base"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.DataStore");
_2.require("dijit._Widget");
_2.require("dojox.wire._base");
_2.declare("dojox.wire.ml.DataStore",_1._Widget,{storeClass:"",postCreate:function(){
this.store=this._createStore();
},_createStore:function(){
if(!this.storeClass){
return null;
}
var _4=_3.wire._getClass(this.storeClass);
if(!_4){
return null;
}
var _5={};
var _6=this.domNode.attributes;
for(var i=0;i<_6.length;i++){
var a=_6.item(i);
if(a.specified&&!this[a.nodeName]){
_5[a.nodeName]=a.nodeValue;
}
}
return new _4(_5);
},getFeatures:function(){
return this.store.getFeatures();
},fetch:function(_7){
return this.store.fetch(_7);
},save:function(_8){
this.store.save(_8);
},newItem:function(_9){
return this.store.newItem(_9);
},deleteItem:function(_a){
return this.store.deleteItem(_a);
},revert:function(){
return this.store.revert();
}});
});
