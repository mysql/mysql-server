//>>built
define("dojox/calendar/Keyboard",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/on","dojo/_base/event","dojo/keys"],function(_1,_2,_3,on,_4,_5){
return _3("dojox.calendar.Keyboard",null,{keyboardUpDownUnit:"minute",keyboardUpDownSteps:15,keyboardLeftRightUnit:"day",keyboardLeftRightSteps:1,allDayKeyboardUpDownUnit:"day",allDayKeyboardUpDownSteps:7,allDayKeyboardLeftRightUnit:"day",allDayKeyboardLeftRightSteps:1,postCreate:function(){
this.inherited(arguments);
this._viewHandles.push(on(this.domNode,"keydown",_2.hitch(this,this._onKeyDown)));
},resizeModifier:"ctrl",maxScrollAnimationDuration:1000,tabIndex:"0",focusedItem:null,_isItemFocused:function(_6){
return this.focusedItem!=null&&this.focusedItem.id==_6.id;
},_setFocusedItemAttr:function(_7){
if(_7!=this.focusedItem){
var _8=this.focusedItem;
this._set("focusedItem",_7);
this.updateRenderers([_8,this.focusedItem],true);
this.onFocusChange({oldValue:_8,newValue:_7});
}
if(_7!=null){
if(this.owner!=null&&this.owner.get("focusedItem")!=null){
this.owner.set("focusedItem",null);
}
if(this._secondarySheet!=null&&this._secondarySheet.set("focusedItem")!=null){
this._secondarySheet.set("focusedItem",null);
}
}
},onFocusChange:function(e){
},showFocus:false,_focusNextItem:function(_9){
if(!this.renderData||!this.renderData.items||this.renderData.items.length==0){
return null;
}
var _a=-1;
var _b=this.renderData.items;
var _c=_b.length-1;
var _d=this.get("focusedItem");
if(_d==null){
_a=_9>0?0:_c;
}else{
_1.some(_b,_2.hitch(this,function(_e,i){
var _f=_e.id==_d.id;
if(_f){
_a=i;
}
return _f;
}));
_a=this._focusNextItemImpl(_9,_a,_c);
}
var _10=false;
var old=-1;
while(old!=_a&&(!_10||_a!=0)){
if(!_10&&_a==0){
_10=true;
}
var _11=_b[_a];
if(this.itemToRenderer[_11.id]!=null){
this.set("focusedItem",_11);
return;
}
old=_a;
_a=this._focusNextItemImpl(_9,_a,_c);
}
},_focusNextItemImpl:function(dir,_12,max){
if(_12==-1){
_12=dir>0?0:max;
}else{
if(_12==0&&dir==-1||_12==max&&dir==1){
return _12;
}
_12=dir>0?++_12:--_12;
}
return _12;
},_handlePrevNextKeyCode:function(e,dir){
if(!this.isLeftToRight()){
dir=dir==1?-1:1;
}
this.showFocus=true;
this._focusNextItem(dir);
var _13=this.get("focusedItem");
if(!e.ctrlKey&&_13){
this.set("selectedItem",_13);
}
if(_13){
this.ensureVisibility(_13.startTime,_13.endTime,"both",undefined,this.maxScrollAnimationDuration);
}
},_keyboardItemEditing:function(e,dir){
_4.stop(e);
var p=this._edProps;
var _14,_15;
if(p.editedItem.allDay||this.roundToDay||p.rendererKind=="label"){
_14=dir=="up"||dir=="down"?this.allDayKeyboardUpDownUnit:this.allDayKeyboardLeftRightUnit;
_15=dir=="up"||dir=="down"?this.allDayKeyboardUpDownSteps:this.allDayKeyboardLeftRightSteps;
}else{
_14=dir=="up"||dir=="down"?this.keyboardUpDownUnit:this.keyboardLeftRightUnit;
_15=dir=="up"||dir=="down"?this.keyboardUpDownSteps:this.keyboardLeftRightSteps;
}
if(dir=="up"||!this.isLeftToRight()&&dir=="right"||this.isLeftToRight()&&dir=="left"){
_15=-_15;
}
var _16=e[this.resizeModifier+"Key"]?"resizeEnd":"move";
var d=_16=="resizeEnd"?p.editedItem.endTime:p.editedItem.startTime;
var _17=this.renderData.dateModule.add(d,_14,_15);
this._startItemEditingGesture([d],_16,"keyboard",e);
this._moveOrResizeItemGesture([_17],"keyboard",e);
this._endItemEditingGesture(_16,"keyboard",e,false);
if(_16=="move"){
if(this.renderData.dateModule.compare(_17,d)==-1){
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"start");
}else{
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"end");
}
}else{
this.ensureVisibility(p.editedItem.startTime,p.editedItem.endTime,"end");
}
},_onKeyDown:function(e){
var _18=this.get("focusedItem");
switch(e.keyCode){
case _5.ESCAPE:
if(this._isEditing){
if(this._editingGesture){
this._endItemEditingGesture("keyboard",e,true);
}
this._endItemEditing("keyboard",true);
this._edProps=null;
}
break;
case _5.SPACE:
_4.stop(e);
if(_18!=null){
this.setItemSelected(_18,e.ctrlKey?!this.isItemSelected(_18):true);
}
break;
case _5.ENTER:
_4.stop(e);
if(_18!=null){
if(this._isEditing){
this._endItemEditing("keyboard",false);
}else{
var _19=this.itemToRenderer[_18.id];
if(_19&&_19.length>0&&this.isItemEditable(_18,_19[0].kind)){
this._edProps={renderer:_19[0],rendererKind:_19[0].kind,tempEditedItem:_18,liveLayout:this.liveLayout};
this.set("selectedItem",_18);
this._startItemEditing(_18,"keyboard");
}
}
}
break;
case _5.LEFT_ARROW:
_4.stop(e);
if(this._isEditing){
this._keyboardItemEditing(e,"left");
}else{
this._handlePrevNextKeyCode(e,-1);
}
break;
case _5.RIGHT_ARROW:
_4.stop(e);
if(this._isEditing){
this._keyboardItemEditing(e,"right");
}else{
this._handlePrevNextKeyCode(e,1);
}
break;
case _5.UP_ARROW:
if(this._isEditing){
this._keyboardItemEditing(e,"up");
}else{
if(this.scrollable){
this.scrollView(-1);
}
}
break;
case _5.DOWN_ARROW:
if(this._isEditing){
this._keyboardItemEditing(e,"down");
}else{
if(this.scrollable){
this.scrollView(1);
}
}
break;
}
}});
});
