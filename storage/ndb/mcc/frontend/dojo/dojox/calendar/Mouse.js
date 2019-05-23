//>>built
define("dojox/calendar/Mouse",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojo/mouse","dojo/on","dojo/keys"],function(_1,_2,_3,_4,_5,_6,_7,on,_8){
return _2("dojox.calendar.Mouse",null,{triggerExtent:3,postMixInProperties:function(){
this.inherited(arguments);
this.on("rendererCreated",_4.hitch(this,function(ir){
var _9=ir.renderer;
var h;
if(!_9.__handles){
_9.__handles=[];
}
h=on(_9.domNode,"click",_4.hitch(this,function(e){
_3.stop(e);
this._onItemClick({triggerEvent:e,renderer:_9,item:this.renderItemToItem(_9.item,this.get("store"))});
}));
_9.__handles.push(h);
h=on(_9.domNode,"dblclick",_4.hitch(this,function(e){
_3.stop(e);
this._onItemDoubleClick({triggerEvent:e,renderer:_9,item:this.renderItemToItem(_9.item,this.get("store"))});
}));
_9.__handles.push(h);
h=on(_9.domNode,"contextmenu",_4.hitch(this,function(e){
this._onItemContextMenu({triggerEvent:e,renderer:_9,item:this.renderItemToItem(_9.item,this.get("store"))});
}));
_9.__handles.push(h);
if(_9.resizeStartHandle){
h=on(_9.resizeStartHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_9,"resizeStart");
}));
_9.__handles.push(h);
}
if(_9.moveHandle){
h=on(_9.moveHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_9,"move");
}));
_9.__handles.push(h);
}
if(_9.resizeEndHandle){
h=on(_9.resizeEndHandle,"mousedown",_4.hitch(this,function(e){
this._onRendererHandleMouseDown(e,_9,"resizeEnd");
}));
_9.__handles.push(h);
}
h=on(_9.domNode,"mousedown",_4.hitch(this,function(e){
this._rendererMouseDownHandler(e,_9);
}));
_9.__handles.push(h);
h=on(ir.container,_7.enter,_4.hitch(this,function(e){
if(!_9.item){
return;
}
if(!this._editingGesture){
this._setHoveredItem(_9.item.item,ir.renderer);
this._onItemRollOver(this.__fixEvt({item:this.renderItemToItem(_9.item,this.get("store")),renderer:_9,triggerEvent:e}));
}
}));
_9.__handles.push(h);
h=on(_9.domNode,_7.leave,_4.hitch(this,function(e){
if(!_9.item){
return;
}
if(!this._editingGesture){
this._setHoveredItem(null);
this._onItemRollOut(this.__fixEvt({item:this.renderItemToItem(_9.item,this.get("store")),renderer:_9,triggerEvent:e}));
}
}));
_9.__handles.push(h);
}));
},_onItemRollOver:function(e){
this._dispatchCalendarEvt(e,"onItemRollOver");
},onItemRollOver:function(e){
},_onItemRollOut:function(e){
this._dispatchCalendarEvt(e,"onItemRollOut");
},onItemRollOut:function(e){
},_rendererMouseDownHandler:function(e,_a){
_3.stop(e);
var _b=this.renderItemToItem(_a.item,this.get("store"));
this.selectFromEvent(e,_b,_a,true);
if(this._setTabIndexAttr){
this[this._setTabIndexAttr].focus();
}
},_onRendererHandleMouseDown:function(e,_c,_d){
_3.stop(e);
this.showFocus=false;
var _e=_c.item;
var _f=_e.item;
if(!this.isItemBeingEdited(_f)){
if(this._isEditing){
this._endItemEditing("mouse",false);
}
this.selectFromEvent(e,this.renderItemToItem(_c.item,this.get("store")),_c,true);
if(this._setTabIndexAttr){
this[this._setTabIndexAttr].focus();
}
this._edProps={editKind:_d,editedItem:_f,rendererKind:_c.rendererKind,tempEditedItem:_f,liveLayout:this.liveLayout};
this.set("focusedItem",this._edProps.editedItem);
}
var _10=[];
_10.push(on(_5.doc,"mouseup",_4.hitch(this,this._editingMouseUpHandler)));
_10.push(on(_5.doc,"mousemove",_4.hitch(this,this._editingMouseMoveHandler)));
var p=this._edProps;
p.handles=_10;
p.eventSource="mouse";
p.editKind=_d;
this._startPoint={x:e.screenX,y:e.screenY};
},_editingMouseMoveHandler:function(e){
var p=this._edProps;
if(this._editingGesture){
if(!this._autoScroll(e.pageX,e.pageY,true)){
this._moveOrResizeItemGesture([this.getTime(e)],"mouse",e);
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
_1.forEach(p.handles,function(_11){
_11.remove();
});
}
},_autoScroll:function(_12,_13,_14){
if(!this.scrollable||!this.autoScroll){
return false;
}
var _15=_6.position(this.scrollContainer,true);
var p=_14?_13-_15.y:_12-_15.x;
var max=_14?_15.h:_15.w;
if(p<0||p>max){
step=Math.floor((p<0?p:p-max)/2)/3;
this._startAutoScroll(step);
return true;
}else{
this._stopAutoScroll();
}
return false;
}});
});
