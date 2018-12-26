//>>built
define("dojox/mdnd/adapter/DndFromDojo",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/_base/html","dojo/_base/window","dojox/mdnd/AreaManager","dojo/dnd/Manager"],function(_1){
var _2=_1.declare("dojox.mdnd.adapter.DndFromDojo",null,{dropIndicatorSize:{"w":0,"h":50},dropIndicatorSize:{"w":0,"h":50},_areaManager:null,_dojoManager:null,_currentArea:null,_oldArea:null,_moveHandler:null,_subscribeHandler:null,constructor:function(){
this._areaManager=dojox.mdnd.areaManager();
this._dojoManager=_1.dnd.manager();
this._currentArea=null;
this._moveHandler=null;
this.subscribeDnd();
},subscribeDnd:function(){
this._subscribeHandler=[_1.subscribe("/dnd/start",this,"onDragStart"),_1.subscribe("/dnd/drop/before",this,"onDrop"),_1.subscribe("/dnd/cancel",this,"onDropCancel"),_1.subscribe("/dnd/source/over",this,"onDndSource")];
},unsubscribeDnd:function(){
_1.forEach(this._subscribeHandler,_1.unsubscribe);
},_getHoverArea:function(_3){
var x=_3.x;
var y=_3.y;
this._oldArea=this._currentArea;
this._currentArea=null;
var _4=this._areaManager._areaList;
for(var i=0;i<_4.length;i++){
var _5=_4[i];
var _6=_5.coords.x;
var _7=_6+_5.node.offsetWidth;
var _8=_5.coords.y;
var _9=_8+_5.node.offsetHeight;
if(_6<=x&&x<=_7&&_8<=y&&y<=_9){
this._areaManager._oldIndexArea=this._areaManager._currentIndexArea;
this._areaManager._currentIndexArea=i;
this._currentArea=_5.node;
break;
}
}
if(this._currentArea!=this._oldArea){
if(this._currentArea==null){
this.onDragExit();
}else{
if(this._oldArea==null){
this.onDragEnter();
}else{
this.onDragExit();
this.onDragEnter();
}
}
}
},onDragStart:function(_a,_b,_c){
this._dragNode=_b[0];
this._copy=_c;
this._source=_a;
this._outSourceHandler=_1.connect(this._dojoManager,"outSource",this,function(){
if(this._moveHandler==null){
this._moveHandler=_1.connect(_1.doc,"mousemove",this,"onMouseMove");
}
});
},onMouseMove:function(e){
var _d={"x":e.pageX,"y":e.pageY};
this._getHoverArea(_d);
if(this._currentArea&&this._areaManager._accept){
if(this._areaManager._dropIndicator.node.style.visibility=="hidden"){
this._areaManager._dropIndicator.node.style.visibility="";
_1.addClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
this._areaManager.placeDropIndicator(_d,this.dropIndicatorSize);
}
},onDragEnter:function(){
var _e=this._dragNode.getAttribute("dndType");
var _f=(_e)?_e.split(/\s*,\s*/):["text"];
this._areaManager._isAccepted(_f,this._areaManager._areaList[this._areaManager._currentIndexArea].accept);
if(this._dojoManager.avatar){
if(this._areaManager._accept){
_1.addClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}else{
_1.removeClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
}
},onDragExit:function(){
this._areaManager._accept=false;
if(this._dojoManager.avatar){
_1.removeClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
if(this._currentArea==null){
this._areaManager._dropMode.refreshItems(this._areaManager._areaList[this._areaManager._oldIndexArea],this._areaManager._oldDropIndex,this.dropIndicatorSize,false);
this._areaManager._resetAfterDrop();
}else{
this._areaManager._dropIndicator.remove();
}
},isAccepted:function(_10,_11){
var _12=(_10.getAttribute("dndType"))?_10.getAttribute("dndType"):"text";
if(_12&&_12 in _11){
return true;
}else{
return false;
}
},onDndSource:function(_13){
if(this._currentArea==null){
return;
}
if(_13){
var _14=false;
if(this._dojoManager.target==_13){
_14=true;
}else{
_14=this.isAccepted(this._dragNode,_13.accept);
}
if(_14){
_1.disconnect(this._moveHandler);
this._currentArea=this._moveHandler=null;
var _15=this._areaManager._dropIndicator.node;
if(_15&&_15.parentNode!==null&&_15.parentNode.nodeType==1){
_15.style.visibility="hidden";
}
}else{
this._resetAvatar();
}
}else{
if(!this._moveHandler){
this._moveHandler=_1.connect(_1.doc,"mousemove",this,"onMouseMove");
}
this._resetAvatar();
}
},_resetAvatar:function(){
if(this._dojoManager.avatar){
if(this._areaManager._accept){
_1.addClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}else{
_1.removeClass(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
}
},onDropCancel:function(){
if(this._currentArea==null){
this._areaManager._resetAfterDrop();
_1.disconnect(this._moveHandler);
_1.disconnect(this._outSourceHandler);
this._currentArea=this._moveHandler=this._outSourceHandler=null;
}else{
if(this._areaManager._accept){
this.onDrop(this._source,[this._dragNode],this._copy,this._currentArea);
}else{
this._currentArea=null;
_1.disconnect(this._outSourceHandler);
_1.disconnect(this._moveHandler);
this._moveHandler=this._outSourceHandler=null;
}
}
},onDrop:function(_16,_17,_18){
_1.disconnect(this._moveHandler);
_1.disconnect(this._outSourceHandler);
this._moveHandler=this._outSourceHandler=null;
if(this._currentArea){
var _19=this._areaManager._currentDropIndex;
_1.publish("/dnd/drop/after",[_16,_17,_18,this._currentArea,_19]);
this._currentArea=null;
}
if(this._areaManager._dropIndicator.node.style.visibility=="hidden"){
this._areaManager._dropIndicator.node.style.visibility="";
}
this._areaManager._resetAfterDrop();
}});
dojox.mdnd.adapter._dndFromDojo=null;
dojox.mdnd.adapter._dndFromDojo=new dojox.mdnd.adapter.DndFromDojo();
return _2;
});
