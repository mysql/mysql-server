//>>built
define("dijit/tree/_dndContainer",["dojo/aspect","dojo/_base/declare","dojo/dom-class","dojo/_base/event","dojo/_base/lang","dojo/mouse","dojo/on"],function(_1,_2,_3,_4,_5,_6,on){
return _2("dijit.tree._dndContainer",null,{constructor:function(_7,_8){
this.tree=_7;
this.node=_7.domNode;
_5.mixin(this,_8);
this.current=null;
this.containerState="";
_3.add(this.node,"dojoDndContainer");
this.events=[on(this.node,_6.enter,_5.hitch(this,"onOverEvent")),on(this.node,_6.leave,_5.hitch(this,"onOutEvent")),_1.after(this.tree,"_onNodeMouseEnter",_5.hitch(this,"onMouseOver"),true),_1.after(this.tree,"_onNodeMouseLeave",_5.hitch(this,"onMouseOut"),true),on(this.node,"dragstart",_5.hitch(_4,"stop")),on(this.node,"selectstart",_5.hitch(_4,"stop"))];
},destroy:function(){
var h;
while(h=this.events.pop()){
h.remove();
}
this.node=this.parent=null;
},onMouseOver:function(_9){
this.current=_9;
},onMouseOut:function(){
this.current=null;
},_changeState:function(_a,_b){
var _c="dojoDnd"+_a;
var _d=_a.toLowerCase()+"State";
_3.replace(this.node,_c+_b,_c+this[_d]);
this[_d]=_b;
},_addItemClass:function(_e,_f){
_3.add(_e,"dojoDndItem"+_f);
},_removeItemClass:function(_10,_11){
_3.remove(_10,"dojoDndItem"+_11);
},onOverEvent:function(){
this._changeState("Container","Over");
},onOutEvent:function(){
this._changeState("Container","");
}});
});
