//>>built
define("dojox/calendar/Touch",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/dom","dojo/dom-geometry","dojo/_base/window","dojo/on","dojo/_base/event","dojo/keys"],function(_1,_2,_3,_4,_5,_6,on,_7,_8){
return _3("dojox.calendar.Touch",null,{touchStartEditingTimer:500,touchEndEditingTimer:10000,postMixInProperties:function(){
this.on("rendererCreated",_2.hitch(this,function(ir){
var _9=ir.renderer;
var h;
if(!_9.__handles){
_9.__handles=[];
}
h=on(_9.domNode,"touchstart",_2.hitch(this,function(e){
this._onRendererTouchStart(e,_9);
}));
_9.__handles.push(h);
}));
},_onRendererTouchStart:function(e,_a){
var p=this._edProps;
if(p&&p.endEditingTimer){
clearTimeout(p.endEditingTimer);
p.endEditingTimer=null;
}
var _b=_a.item.item;
if(p&&p.endEditingTimer){
clearTimeout(p.endEditingTimer);
p.endEditingTimer=null;
}
if(p!=null&&p.item!=_b){
if(p.startEditingTimer){
clearTimeout(p.startEditingTimer);
}
this._endItemEditing("touch",false);
p=null;
}
if(!p){
var _c=[];
_c.push(on(_6.doc,"touchend",_2.hitch(this,this._docEditingTouchEndHandler)));
_c.push(on(this.itemContainer,"touchmove",_2.hitch(this,this._docEditingTouchMoveHandler)));
this._setEditingProperties({touchMoved:false,item:_b,renderer:_a,rendererKind:_a.rendererKind,event:e,handles:_c,liveLayout:this.liveLayout});
p=this._edProps;
}
if(this._isEditing){
_2.mixin(p,this._getTouchesOnRenderers(e,p.editedItem));
this._startTouchItemEditingGesture(e);
}else{
if(e.touches.length>1){
_7.stop(e);
return;
}
this._touchSelectionTimer=setTimeout(_2.hitch(this,function(){
this._saveSelectedItems=this.get("selectedItems");
var _d=this.selectFromEvent(e,this.renderItemToItem(_b,this.get("store")),_a,false);
if(_d){
this._pendingSelectedItem=_b;
}else{
delete this._saveSelectedItems;
}
this._touchSelectionTimer=null;
}),200);
p.start={x:e.touches[0].screenX,y:e.touches[0].screenY};
if(this.isItemEditable(p.item,p.rendererKind)){
this._edProps.startEditingTimer=setTimeout(_2.hitch(this,function(){
if(this._touchSelectionTimer){
clearTimeout(this._touchSelectionTimer);
delete this._touchSelectionTime;
}
if(this._pendingSelectedItem){
this.dispatchChange(this._saveSelectedItems==null?null:this._saveSelectedItems[0],this._pendingSelectedItem,null,e);
delete this._saveSelectedItems;
delete this._pendingSelectedItem;
}else{
this.selectFromEvent(e,this.renderItemToItem(_b,this.get("store")),_a);
}
this._startItemEditing(p.item,"touch",e);
p.moveTouchIndex=0;
this._startItemEditingGesture([this.getTime(e)],"move","touch",e);
}),this.touchStartEditingTimer);
}
}
},_docEditingTouchMoveHandler:function(e){
var p=this._edProps;
var _e={x:e.touches[0].screenX,y:e.touches[0].screenY};
if(p.startEditingTimer&&(Math.abs(_e.x-p.start.x)>25||Math.abs(_e.y-p.start.y)>25)){
clearTimeout(p.startEditingTimer);
p.startEditingTimer=null;
clearTimeout(this._touchSelectionTimer);
this._touchSelectionTimer=null;
if(this._pendingSelectedItem){
delete this._pendingSelectedItem;
this.selectFromEvent(e,null,null,false);
}
}
p.touchMoved=true;
if(this._editingGesture){
_7.stop(e);
if(p.itemBeginDispatched){
var _f=[];
var d=p.editKind=="resizeEnd"?p.editedItem.endTime:p.editedItem.startTime;
switch(p.editKind){
case "move":
var _10=p.moveTouchIndex==null||p.moveTouchIndex<0?0:p.moveTouchIndex;
_f[0]=this.getTime(e,-1,-1,_10);
break;
case "resizeStart":
_f[0]=this.getTime(e,-1,-1,p.resizeStartTouchIndex);
break;
case "resizeEnd":
_f[0]=this.getTime(e,-1,-1,p.resizeEndTouchIndex);
break;
case "resizeBoth":
_f[0]=this.getTime(e,-1,-1,p.resizeStartTouchIndex);
_f[1]=this.getTime(e,-1,-1,p.resizeEndTouchIndex);
break;
}
this._moveOrResizeItemGesture(_f,"touch",e);
if(p.editKind=="move"){
if(this.renderData.dateModule.compare(p.editedItem.startTime,d)==-1){
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"start",this.autoScrollTouchMargin);
}else{
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"end",this.autoScrollTouchMargin);
}
}else{
if(e.editKind=="resizeStart"||e.editKind=="resizeBoth"){
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"start",this.autoScrollTouchMargin);
}else{
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"end",this.autoScrollTouchMargin);
}
}
}
}
},autoScrollTouchMargin:10,_docEditingTouchEndHandler:function(e){
_7.stop(e);
var p=this._edProps;
if(p.startEditingTimer){
clearTimeout(p.startEditingTimer);
p.startEditingTimer=null;
}
if(this._isEditing){
_2.mixin(p,this._getTouchesOnRenderers(e,p.editedItem));
if(this._editingGesture){
if(p.touchesLen==0){
this._endItemEditingGesture("touch",e);
if(this.touchEndEditingTimer>0){
p.endEditingTimer=setTimeout(_2.hitch(this,function(){
this._endItemEditing("touch",false);
}),this.touchEndEditingTimer);
}
}else{
if(this._editingGesture){
this._endItemEditingGesture("touch",e);
}
this._startTouchItemEditingGesture(e);
}
}
}else{
if(!p.touchMoved){
_7.stop(e);
_1.forEach(p.handles,function(_11){
_11.remove();
});
if(this._touchSelectionTimer){
clearTimeout(this._touchSelectionTimer);
this.selectFromEvent(e,this.renderItemToItem(p.item,this.get("store")),p.renderer,true);
}else{
if(this._pendingSelectedItem){
this.dispatchChange(this._saveSelectedItems.length==0?null:this._saveSelectedItems[0],this._pendingSelectedItem,null,e);
delete this._saveSelectedItems;
delete this._pendingSelectedItem;
}
}
if(this._pendingDoubleTap&&this._pendingDoubleTap.item==p.item){
this._onItemDoubleClick({triggerEvent:e,renderer:p.renderer,item:this.renderItemToItem(p.item,this.get("store"))});
clearTimeout(this._pendingDoubleTap.timer);
delete this._pendingDoubleTap;
}else{
this._pendingDoubleTap={item:p.item,timer:setTimeout(_2.hitch(this,function(){
delete this._pendingDoubleTap;
}),this.doubleTapDelay)};
this._onItemClick({triggerEvent:e,renderer:p.renderer,item:this.renderItemToItem(p.item,this.get("store"))});
}
this._edProps=null;
}else{
if(this._saveSelectedItems){
this.set("selectedItems",this._saveSelectedItems);
delete this._saveSelectedItems;
delete this._pendingSelectedItem;
}
_1.forEach(p.handles,function(_12){
_12.remove();
});
this._edProps=null;
}
}
},_startTouchItemEditingGesture:function(e){
var p=this._edProps;
var _13=p.resizeStartTouchIndex!=-1;
var _14=p.resizeEndTouchIndex!=-1;
if(_13&&_14||this._editingGesture&&p.touchesLen==2&&(_14&&p.editKind=="resizeStart"||_13&&p.editKind=="resizeEnd")){
if(this._editingGesture&&p.editKind!="resizeBoth"){
this._endItemEditingGesture("touch",e);
}
p.editKind="resizeBoth";
this._startItemEditingGesture([this.getTime(e,-1,-1,p.resizeStartTouchIndex),this.getTime(e,-1,-1,p.resizeEndTouchIndex)],p.editKind,"touch",e);
return;
}else{
if(_13&&p.touchesLen==1&&!this._editingGesture){
this._startItemEditingGesture([this.getTime(e,-1,-1,p.resizeStartTouchIndex)],"resizeStart","touch",e);
return;
}else{
if(_14&&p.touchesLen==1&&!this._editingGesture){
this._startItemEditingGesture([this.getTime(e,-1,-1,p.resizeEndTouchIndex)],"resizeEnd","touch",e);
return;
}else{
this._startItemEditingGesture([this.getTime(e)],"move","touch",e);
}
}
}
},_getTouchesOnRenderers:function(e,_15){
var irs=this._getStartEndRenderers(_15);
var _16=-1;
var _17=-1;
var _18=-1;
var _19=irs[0]!=null&&irs[0].resizeStartHandle!=null;
var _1a=irs[1]!=null&&irs[1].resizeEndHandle!=null;
var len=0;
var _1b=false;
var _1c=this.itemToRenderer[_15.id];
for(var i=0;i<e.touches.length;i++){
if(_16==-1&&_19){
_1b=_4.isDescendant(e.touches[i].target,irs[0].resizeStartHandle);
if(_1b){
_16=i;
len++;
}
}
if(_17==-1&&_1a){
_1b=_4.isDescendant(e.touches[i].target,irs[1].resizeEndHandle);
if(_1b){
_17=i;
len++;
}
}
if(_16==-1&&_17==-1){
for(var j=0;j<_1c.length;j++){
_1b=_4.isDescendant(e.touches[i].target,_1c[j].container);
if(_1b){
_18=i;
len++;
break;
}
}
}
if(_16!=-1&&_17!=-1&&_18!=-1){
break;
}
}
return {touchesLen:len,resizeStartTouchIndex:_16,resizeEndTouchIndex:_17,moveTouchIndex:_18};
}});
});
