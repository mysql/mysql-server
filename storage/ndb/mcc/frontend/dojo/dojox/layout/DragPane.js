//>>built
define("dojox/layout/DragPane",["dojo/_base/declare","dijit/_Widget","dojo/_base/html","dojo/dom-style"],function(_1,_2,_3,_4){
return _1("dojox.layout.DragPane",_2,{invert:true,postCreate:function(){
this.connect(this.domNode,"onmousedown","_down");
this.connect(this.domNode,"onmouseleave","_up");
this.connect(this.domNode,"onmouseup","_up");
},_down:function(e){
var t=this.domNode;
e.preventDefault();
_4.set(t,"cursor","move");
this._x=e.pageX;
this._y=e.pageY;
if((this._x<t.offsetLeft+t.clientWidth)&&(this._y<t.offsetTop+t.clientHeight)){
_3.setSelectable(t,false);
this._mover=this.connect(t,"onmousemove","_move");
}
},_up:function(e){
_3.setSelectable(this.domNode,true);
_4.set(this.domNode,"cursor","pointer");
this._mover&&this.disconnect(this._mover);
delete this._mover;
},_move:function(e){
var _5=this.invert?1:-1;
this.domNode.scrollTop+=(this._y-e.pageY)*_5;
this.domNode.scrollLeft+=(this._x-e.pageX)*_5;
this._x=e.pageX;
this._y=e.pageY;
}});
});
