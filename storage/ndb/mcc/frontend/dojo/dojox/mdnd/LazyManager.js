//>>built
define("dojox/mdnd/LazyManager",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-attr","dojo/dnd/common","dojo/dnd/Manager","./PureSource","dojo/_base/unload"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mdnd.LazyManager",null,{constructor:function(){
this._registry={};
this._fakeSource=new _9(_5.create("div"),{"copyOnly":false});
this._fakeSource.startup();
_1.addOnUnload(_3.hitch(this,"destroy"));
this.manager=_8.manager();
},getItem:function(_a){
var _b=_a.getAttribute("dndType");
return {"data":_a.getAttribute("dndData")||_a.innerHTML,"type":_b?_b.split(/\s*,\s*/):["text"]};
},startDrag:function(e,_c){
_c=_c||e.target;
if(_c){
var m=this.manager,_d=this.getItem(_c);
if(_c.id==""){
_6.set(_c,"id",_7.getUniqueId());
}
_4.add(_c,"dojoDndItem");
this._fakeSource.setItem(_c.id,_d);
m.startDrag(this._fakeSource,[_c],false);
m.onMouseMove(e);
}
},cancelDrag:function(){
var m=this.manager;
m.target=null;
m.onMouseUp();
},destroy:function(){
this._fakeSource.destroy();
}});
});
