//>>built
define("dojox/mdnd/adapter/DndToDojo",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/topic","dojo/_base/window","dojox/mdnd/PureSource","dojox/mdnd/LazyManager","dojox/mdnd/AreaManager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_2("dojox.mdnd.adapter.DndToDojo",null,{_dojoList:null,_currentDojoArea:null,_dojoxManager:null,_dragStartHandler:null,_dropHandler:null,_moveHandler:null,_moveUpHandler:null,_draggedNode:null,constructor:function(){
this._dojoList=[];
this._currentDojoArea=null;
this._dojoxManager=_c.areaManager();
this._dragStartHandler=_3.subscribe("/dojox/mdnd/drag/start",this,function(_e,_f,_10){
this._draggedNode=_e;
this._moveHandler=_3.connect(_1.doc,"onmousemove",this,"onMouseMove");
});
this._dropHandler=_3.subscribe("/dojox/mdnd/drop",this,function(_11,_12,_13){
if(this._currentDojoArea){
_3.publish("/dojox/mdnd/adapter/dndToDojo/cancel",[this._currentDojoArea.node,this._currentDojoArea.type,this._draggedNode,this.accept]);
}
this._draggedNode=null;
this._currentDojoArea=null;
_3.disconnect(this._moveHandler);
});
},_getIndexDojoArea:function(_14){
if(_14){
for(var i=0,l=this._dojoList.length;i<l;i++){
if(this._dojoList[i].node===_14){
return i;
}
}
}
return -1;
},_initCoordinates:function(_15){
if(_15){
var _16=_7.position(_15,true),_17={};
_17.x=_16.x;
_17.y=_16.y;
_17.x1=_16.x+_16.w;
_17.y1=_16.y+_16.h;
return _17;
}
return null;
},register:function(_18,_19,_1a){
if(this._getIndexDojoArea(_18)==-1){
var _1b=this._initCoordinates(_18),_1c={"node":_18,"type":_19,"dojo":(_1a)?_1a:false,"coords":_1b};
this._dojoList.push(_1c);
if(_1a&&!this._lazyManager){
this._lazyManager=new dojox.mdnd.LazyManager();
}
}
},unregisterByNode:function(_1d){
var _1e=this._getIndexDojoArea(_1d);
if(_1e!=-1){
this._dojoList.splice(_1e,1);
}
},unregisterByType:function(_1f){
if(_1f){
var _20=[];
_4.forEach(this._dojoList,function(_21,i){
if(_21.type!=_1f){
_20.push(_21);
}
});
this._dojoList=_20;
}
},unregister:function(){
this._dojoList=[];
},refresh:function(){
var _22=this._dojoList;
this.unregister();
_4.forEach(_22,function(_23){
_23.coords=this._initCoordinates(_23.node);
},this);
this._dojoList=_22;
},refreshByType:function(_24){
var _25=this._dojoList;
this.unregister();
_4.forEach(_25,function(_26){
if(_26.type==_24){
_26.coords=this._initCoordinates(_26.node);
}
},this);
this._dojoList=_25;
},_getHoverDojoArea:function(_27){
this._oldDojoArea=this._currentDojoArea;
this._currentDojoArea=null;
var x=_27.x;
var y=_27.y;
var _28=this._dojoList.length;
for(var i=0;i<_28;i++){
var _29=this._dojoList[i];
var _2a=_29.coords;
if(_2a.x<=x&&x<=_2a.x1&&_2a.y<=y&&y<=_2a.y1){
this._currentDojoArea=_29;
break;
}
}
},onMouseMove:function(e){
var _2b={"x":e.pageX,"y":e.pageY};
this._getHoverDojoArea(_2b);
if(this._currentDojoArea!=this._oldDojoArea){
if(this._currentDojoArea==null){
this.onDragExit(e);
}else{
if(this._oldDojoArea==null){
this.onDragEnter(e);
}else{
this.onDragExit(e);
this.onDragEnter(e);
}
}
}
},isAccepted:function(_2c,_2d){
return true;
},onDragEnter:function(e){
if(this._currentDojoArea.dojo){
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
_3.disconnect(this._dojoxManager._dragItem.item.events.pop());
_9.body().removeChild(this._dojoxManager._cover);
_9.body().removeChild(this._dojoxManager._cover2);
var _2e=this._dojoxManager._dragItem.item.node;
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.unsubscribeDnd();
}
_6.set(_2e,{"position":"relative","top":"0","left":"0"});
this._lazyManager.startDrag(e,_2e);
var _2f=_3.connect(this._lazyManager.manager,"overSource",this,function(){
_3.disconnect(_2f);
if(this._lazyManager.manager.canDropFlag){
this._dojoxManager._dropIndicator.node.style.display="none";
}
});
this.cancelHandler=_3.subscribe("/dnd/cancel",this,function(){
var _30=this._dojoxManager._dragItem.item;
_30.events=[_3.connect(_30.handle,"onmousedown",_30,"onMouseDown")];
_9.body().appendChild(this._dojoxManager._cover);
_9.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_30.node);
var _31=this._dojoxManager._areaList[this._dojoxManager._sourceIndexArea];
var _32=this._dojoxManager._sourceDropIndex;
var _33=null;
if(_32!=_31.items.length&&_32!=-1){
_33=_31.items[this._dojoxManager._sourceDropIndex].item.node;
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display=="";
}
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._draggedNode.style.display="";
this._dojoxManager.onDrop(this._draggedNode);
_3.unsubscribe(this.cancelHandler);
_3.unsubscribe(this.dropHandler);
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.subscribeDnd();
}
});
this.dropHandler=_3.subscribe("/dnd/drop/before",this,function(_34){
_3.unsubscribe(this.cancelHandler);
_3.unsubscribe(this.dropHandler);
this.onDrop();
});
}else{
this.accept=this.isAccepted(this._dojoxManager._dragItem.item.node,this._currentDojoArea);
if(this.accept){
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
this._dojoxManager._dropIndicator.node.style.display="none";
if(!this._moveUpHandler){
this._moveUpHandler=_3.connect(_1.doc,"onmouseup",this,"onDrop");
}
}
}
_3.publish("/dojox/mdnd/adapter/dndToDojo/over",[this._currentDojoArea.node,this._currentDojoArea.type,this._draggedNode,this.accept]);
},onDragExit:function(e){
if(this._oldDojoArea.dojo){
_3.unsubscribe(this.cancelHandler);
_3.unsubscribe(this.dropHandler);
var _35=this._dojoxManager._dragItem.item;
this._dojoxManager._dragItem.item.events.push(_3.connect(_35.node.ownerDocument,"onmousemove",_35,"onMove"));
_9.body().appendChild(this._dojoxManager._cover);
_9.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_35.node);
var _36=_35.node.style;
_36.position="absolute";
_36.left=(_35.offsetDrag.l+e.pageX)+"px";
_36.top=(_35.offsetDrag.t+e.pageX)+"px";
_36.display="";
this._lazyManager.cancelDrag();
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.subscribeDnd();
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display="";
}
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._dojoxManager._dragItem.item.onMove(e);
}else{
if(this.accept){
if(this._moveUpHandler){
_3.disconnect(this._moveUpHandler);
this._moveUpHandler=null;
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display="";
}
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_3.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._dojoxManager._dragItem.item.onMove(e);
}
}
_3.publish("/dojox/mdnd/adapter/dndToDojo/out",[this._oldDojoArea.node,this._oldDojoArea.type,this._draggedNode,this.accept]);
},onDrop:function(e){
if(this._currentDojoArea.dojo){
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.subscribeDnd();
}
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display="";
}
if(this._dojoxManager._cover.parentNode&&this._dojoxManager._cover.parentNode.nodeType==1){
_9.body().removeChild(this._dojoxManager._cover);
_9.body().removeChild(this._dojoxManager._cover2);
}
if(this._draggedNode.parentNode==this._dojoxManager._cover){
this._dojoxManager._cover.removeChild(this._draggedNode);
}
_3.disconnect(this._moveHandler);
_3.disconnect(this._moveUpHandler);
this._moveHandler=this._moveUpHandler=null;
_3.publish("/dojox/mdnd/adapter/dndToDojo/drop",[this._draggedNode,this._currentDojoArea.node,this._currentDojoArea.type]);
_5.remove(this._draggedNode,"dragNode");
var _37=this._draggedNode.style;
_37.position="relative";
_37.left="0";
_37.top="0";
_37.width="auto";
_4.forEach(this._dojoxManager._dragItem.handlers,_3.disconnect);
this._dojoxManager._deleteMoveableItem(this._dojoxManager._dragItem);
this._draggedNode=null;
this._currentDojoArea=null;
this._dojoxManager._resetAfterDrop();
}});
dojox.mdnd.adapter._dndToDojo=null;
dojox.mdnd.adapter.dndToDojo=function(){
if(!dojox.mdnd.adapter._dndToDojo){
dojox.mdnd.adapter._dndToDojo=new dojox.mdnd.adapter.DndToDojo();
}
return dojox.mdnd.adapter._dndToDojo;
};
return _d;
});
