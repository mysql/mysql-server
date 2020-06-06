//>>built
define("dojox/calendar/Mouse",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojo/mouse","dojo/on","dojo/keys"],function(_1,_2,_3,_4,_5,_6,_7,on,_8){
return _2("dojox.calendar.Mouse",null,{triggerExtent:3,postMixInProperties:function(){
this.inherited(arguments);
this.on("rendererCreated",_4.hitch(this,function(_9){
var _a=_9.renderer.renderer;
this.own(on(_a.domNode,"click",_4.hitch(this,function(e){
_3.stop(e);
this._onItemClick({triggerEvent:e,renderer:_a,item:_a.item._item});
})));
this.own(on(_a.domNode,"dblclick",_4.hitch(this,function(e){
_3.stop(e);
this._onItemDoubleClick({triggerEvent:e,renderer:_a,item:_a.item._item});
})));
this.own(on(_a.domNode,"contextmenu",_4.hitch(this,function(e){
this._onItemContextMenu({triggerEvent:e,renderer:_a,item:_a.item._item});
})));
if(_a.resizeStartHandle){
this.own(on(_a.resizeStartHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_a,"resizeStart");
})));
}
if(_a.moveHandle){
this.own(on(_a.moveHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_a,"move");
})));
}
if(_a.resizeEndHandle){
this.own(on(_a.resizeEndHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_a,"resizeEnd");
})));
}
this.own(on(_a.domNode,"mousedown",_4.hitch(this,function(e){
this._rendererMouseDownHandler(e,_a);
})));
this.own(on(_9.renderer.container,_7.enter,_4.hitch(this,function(e){
if(!_a.item){
return;
}
if(!this._editingGesture){
this._setHoveredItem(_a.item.item,_a);
this._onItemRollOver(this.__fixEvt({item:_a.item._item,renderer:_a,triggerEvent:e}));
}
})));
this.own(on(_a.domNode,_7.leave,_4.hitch(this,function(e){
if(!_a.item){
return;
}
if(!this._editingGesture){
this._setHoveredItem(null);
this._onItemRollOut(this.__fixEvt({item:_a.item._item,renderer:_a,triggerEvent:e}));
}
})));
}));
},_onItemRollOver:function(e){
this._dispatchCalendarEvt(e,"onItemRollOver");
},onItemRollOver:function(e){
},_onItemRollOut:function(e){
this._dispatchCalendarEvt(e,"onItemRollOut");
},onItemRollOut:function(e){
},_rendererMouseDownHandler:function(e,_b){
_3.stop(e);
var _c=_b.item._item;
this.selectFromEvent(e,_c,_b,true);
if(this._setTabIndexAttr){
this[this._setTabIndexAttr].focus();
}
},_onRendererHandleMouseDown:function(e,_d,_e){
_3.stop(e);
this.showFocus=false;
var _f=_d.item;
var _10=_f.item;
if(!this.isItemBeingEdited(_10)){
if(this._isEditing){
this._endItemEditing("mouse",false);
}
this.selectFromEvent(e,_d.item._item,_d,true);
if(this._setTabIndexAttr){
this[this._setTabIndexAttr].focus();
}
this._edProps={editKind:_e,editedItem:_10,rendererKind:_d.rendererKind,tempEditedItem:_10,liveLayout:this.liveLayout};
this.set("focusedItem",this._edProps.editedItem);
}
var _11=[];
_11.push(on(_5.doc,"mouseup",_4.hitch(this,this._editingMouseUpHandler)));
_11.push(on(_5.doc,"mousemove",_4.hitch(this,this._editingMouseMoveHandler)));
var p=this._edProps;
p.handles=_11;
p.eventSource="mouse";
p.editKind=_e;
this._startPoint={x:e.screenX,y:e.screenY};
},_editingMouseMoveHandler:function(e){
var p=this._edProps;
if(this._editingGesture){
if(!this._autoScroll(e.pageX,e.pageY,true)){
this._moveOrResizeItemGesture([this.getTime(e)],"mouse",e,this.getSubColumn(e));
}
}else{
if(Math.abs(this._startPoint.x-e.screenX)>=this.triggerExtent||Math.abs(this._startPoint.y-e.screenY)>=this.triggerExtent){
if(!this._isEditing){
this._startItemEditing(p.editedItem,"mouse");
}
p=this._edProps;
this._startItemEditingGesture([this.getTime(e)],p.editKind,"mouse",e);
}
}
},_editingMouseUpHandler:function(e){
var p=this._edProps;
this._stopAutoScroll();
if(this._isEditing){
if(this._editingGesture){
this._endItemEditingGesture("mouse",e);
}
this._endItemEditing("mouse",false);
}else{
_1.forEach(p.handles,function(_12){
_12.remove();
});
}
},_autoScroll:function(_13,_14,_15){
if(!this.scrollable||!this.autoScroll){
return false;
}
var _16=_6.position(this.scrollContainer,true);
var p=_15?_14-_16.y:_13-_16.x;
var max=_15?_16.h:_16.w;
if(p<0||p>max){
var _17=Math.floor((p<0?p:p-max)/2)/3;
this._startAutoScroll(_17);
return true;
}else{
this._stopAutoScroll();
}
return false;
}});
});
