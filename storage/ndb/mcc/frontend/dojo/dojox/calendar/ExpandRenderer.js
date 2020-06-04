//>>built
require({cache:{"url:dojox/calendar/templates/ExpandRenderer.html":"<div class=\"dojoxCalendarExpand\" onselectstart=\"return false;\" data-dojo-attach-event=\"click:_onClick,touchstart:_onMouseDown,touchend:_onClick,mousedown:_onMouseDown,mouseup:_onMouseUp,mouseover:_onMouseOver,mouseout:_onMouseOut\">\n\t<div class=\"bg\"><span data-dojo-attach-point=\"expand\">▼</span><span style=\"display:none\" data-dojo-attach-point=\"collapse\">▲</span></div>\t\n</div>\n"}});
define("dojox/calendar/ExpandRenderer",["dojo/_base/declare","dojo/_base/lang","dojo/_base/event","dojo/_base/window","dojo/on","dojo/dom-class","dojo/dom-style","dijit/_WidgetBase","dijit/_TemplatedMixin","dojo/text!./templates/ExpandRenderer.html"],function(_1,_2,_3,_4,on,_5,_6,_7,_8,_9){
return _1("dojox.calendar.ExpandRenderer",[_7,_8],{templateString:_9,baseClass:"dojoxCalendarExpand",owner:null,focused:false,up:false,down:false,date:null,items:null,rowIndex:-1,columnIndex:-1,_setExpandedAttr:function(_a){
_6.set(this.expand,"display",_a?"none":"inline-block");
_6.set(this.collapse,"display",_a?"inline-block":"none");
this._set("expanded",_a);
},_setDownAttr:function(_b){
this._setState("down",_b,"Down");
},_setUpAttr:function(_c){
this._setState("up",_c,"Up");
},_setFocusedAttr:function(_d){
this._setState("focused",_d,"Focused");
},_setState:function(_e,_f,_10){
if(this[_e]!=_f){
var tn=this.stateNode||this.domNode;
_5[_f?"add":"remove"](tn,_10);
this._set(_e,_f);
}
},_onClick:function(e){
if(this.owner&&this.owner.expandRendererClickHandler){
this.owner.expandRendererClickHandler(e,this);
}
},_onMouseDown:function(e){
_3.stop(e);
this.set("down",true);
},_onMouseUp:function(e){
this.set("down",false);
},_onMouseOver:function(e){
if(!this.up){
var _11=e.button==1;
this.set("up",!_11);
this.set("down",_11);
}
},_onMouseOut:function(e){
var _12=e.relatedTarget;
while(_12!=e.currentTarget&&_12!=_4.doc.body&&_12!=null){
_12=_12.parentNode;
}
if(_12==e.currentTarget){
return;
}
this.set("up",false);
this.set("down",false);
}});
});
