//>>built
define("dojox/mdnd/adapter/DndFromDojo",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-class","dojo/_base/window","dojox/mdnd/AreaManager","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2("dojox.mdnd.adapter.DndFromDojo",null,{dropIndicatorSize:{"w":0,"h":50},dropIndicatorSize:{"w":0,"h":50},_areaManager:null,_dojoManager:null,_currentArea:null,_oldArea:null,_moveHandler:null,_subscribeHandler:null,constructor:function(){
this._areaManager=dojox.mdnd.areaManager();
this._dojoManager=_8.manager();
this._currentArea=null;
this._moveHandler=null;
this.subscribeDnd();
},subscribeDnd:function(){
this._subscribeHandler=[_3.subscribe("/dnd/start",this,"onDragStart"),_3.subscribe("/dnd/drop/before",this,"onDrop"),_3.subscribe("/dnd/cancel",this,"onDropCancel"),_3.subscribe("/dnd/source/over",this,"onDndSource")];
},unsubscribeDnd:function(){
_4.forEach(this._subscribeHandler,_3.unsubscribe);
},_getHoverArea:function(_a){
var x=_a.x;
var y=_a.y;
this._oldArea=this._currentArea;
this._currentArea=null;
var _b=this._areaManager._areaList;
for(var i=0;i<_b.length;i++){
var _c=_b[i];
var _d=_c.coords.x;
var _e=_d+_c.node.offsetWidth;
var _f=_c.coords.y;
var _10=_f+_c.node.offsetHeight;
if(_d<=x&&x<=_e&&_f<=y&&y<=_10){
this._areaManager._oldIndexArea=this._areaManager._currentIndexArea;
this._areaManager._currentIndexArea=i;
this._currentArea=_c.node;
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
},onDragStart:function(_11,_12,_13){
this._dragNode=_12[0];
this._copy=_13;
this._source=_11;
this._outSourceHandler=_3.connect(this._dojoManager,"outSource",this,function(){
if(this._moveHandler==null){
this._moveHandler=_3.connect(_1.doc,"mousemove",this,"onMouseMove");
}
});
},onMouseMove:function(e){
var _14={"x":e.pageX,"y":e.pageY};
this._getHoverArea(_14);
if(this._currentArea&&this._areaManager._accept){
if(this._areaManager._dropIndicator.node.style.visibility=="hidden"){
this._areaManager._dropIndicator.node.style.visibility="";
_5.add(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
this._areaManager.placeDropIndicator(_14,this.dropIndicatorSize);
}
},onDragEnter:function(){
var _15=this._dragNode.getAttribute("dndType");
var _16=(_15)?_15.split(/\s*,\s*/):["text"];
this._areaManager._isAccepted(_16,this._areaManager._areaList[this._areaManager._currentIndexArea].accept);
if(this._dojoManager.avatar){
if(this._areaManager._accept){
_5.add(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}else{
_5.remove(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
}
},onDragExit:function(){
this._areaManager._accept=false;
if(this._dojoManager.avatar){
_5.remove(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
if(this._currentArea==null){
this._areaManager._dropMode.refreshItems(this._areaManager._areaList[this._areaManager._oldIndexArea],this._areaManager._oldDropIndex,this.dropIndicatorSize,false);
this._areaManager._resetAfterDrop();
}else{
this._areaManager._dropIndicator.remove();
}
},isAccepted:function(_17,_18){
var _19=(_17.getAttribute("dndType"))?_17.getAttribute("dndType"):"text";
if(_19&&_19 in _18){
return true;
}else{
return false;
}
},onDndSource:function(_1a){
if(this._currentArea==null){
return;
}
if(_1a){
var _1b=false;
if(this._dojoManager.target==_1a){
_1b=true;
}else{
_1b=this.isAccepted(this._dragNode,_1a.accept);
}
if(_1b){
_3.disconnect(this._moveHandler);
this._currentArea=this._moveHandler=null;
var _1c=this._areaManager._dropIndicator.node;
if(_1c&&_1c.parentNode!==null&&_1c.parentNode.nodeType==1){
_1c.style.visibility="hidden";
}
}else{
this._resetAvatar();
}
}else{
if(!this._moveHandler){
this._moveHandler=_3.connect(_1.doc,"mousemove",this,"onMouseMove");
}
this._resetAvatar();
}
},_resetAvatar:function(){
if(this._dojoManager.avatar){
if(this._areaManager._accept){
_5.add(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}else{
_5.remove(this._dojoManager.avatar.node,"dojoDndAvatarCanDrop");
}
}
},onDropCancel:function(){
if(this._currentArea==null){
this._areaManager._resetAfterDrop();
_3.disconnect(this._moveHandler);
_3.disconnect(this._outSourceHandler);
this._currentArea=this._moveHandler=this._outSourceHandler=null;
}else{
if(this._areaManager._accept){
this.onDrop(this._source,[this._dragNode],this._copy,this._currentArea);
}else{
this._currentArea=null;
_3.disconnect(this._outSourceHandler);
_3.disconnect(this._moveHandler);
this._moveHandler=this._outSourceHandler=null;
}
}
},onDrop:function(_1d,_1e,_1f){
_3.disconnect(this._moveHandler);
_3.disconnect(this._outSourceHandler);
this._moveHandler=this._outSourceHandler=null;
if(this._currentArea){
var _20=this._areaManager._currentDropIndex;
_3.publish("/dnd/drop/after",[_1d,_1e,_1f,this._currentArea,_20]);
this._currentArea=null;
}
if(this._areaManager._dropIndicator.node.style.visibility=="hidden"){
this._areaManager._dropIndicator.node.style.visibility="";
}
this._areaManager._resetAfterDrop();
}});
dojox.mdnd.adapter._dndFromDojo=null;
dojox.mdnd.adapter._dndFromDojo=new dojox.mdnd.adapter.DndFromDojo();
return _9;
});
