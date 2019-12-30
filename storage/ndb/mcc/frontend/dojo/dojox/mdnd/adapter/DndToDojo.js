//>>built
define("dojox/mdnd/adapter/DndToDojo",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/topic","dojo/_base/window","dojox/mdnd/PureSource","dojox/mdnd/LazyManager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_2("dojox.mdnd.adapter.DndToDojo",null,{_dojoList:null,_currentDojoArea:null,_dojoxManager:null,_dragStartHandler:null,_dropHandler:null,_moveHandler:null,_moveUpHandler:null,_draggedNode:null,constructor:function(){
this._dojoList=[];
this._currentDojoArea=null;
this._dojoxManager=dojox.mdnd.areaManager();
this._dragStartHandler=_3.subscribe("/dojox/mdnd/drag/start",this,function(_b,_c,_d){
this._draggedNode=_b;
this._moveHandler=_3.connect(_1.doc,"onmousemove",this,"onMouseMove");
});
this._dropHandler=_3.subscribe("/dojox/mdnd/drop",this,function(_e,_f,_10){
if(this._currentDojoArea){
_3.publish("/dojox/mdnd/adapter/dndToDojo/cancel",[this._currentDojoArea.node,this._currentDojoArea.type,this._draggedNode,this.accept]);
}
this._draggedNode=null;
this._currentDojoArea=null;
_3.disconnect(this._moveHandler);
});
},_getIndexDojoArea:function(_11){
if(_11){
for(var i=0,l=this._dojoList.length;i<l;i++){
if(this._dojoList[i].node===_11){
return i;
}
}
}
return -1;
},_initCoordinates:function(_12){
if(_12){
var _13=_7.position(_12,true),_14={};
_14.x=_13.x;
_14.y=_13.y;
_14.x1=_13.x+_13.w;
_14.y1=_13.y+_13.h;
return _14;
}
return null;
},register:function(_15,_16,_17){
if(this._getIndexDojoArea(_15)==-1){
var _18=this._initCoordinates(_15),_19={"node":_15,"type":_16,"dojo":(_17)?_17:false,"coords":_18};
this._dojoList.push(_19);
if(_17&&!this._lazyManager){
this._lazyManager=new dojox.mdnd.LazyManager();
}
}
},unregisterByNode:function(_1a){
var _1b=this._getIndexDojoArea(_1a);
if(_1b!=-1){
this._dojoList.splice(_1b,1);
}
},unregisterByType:function(_1c){
if(_1c){
var _1d=[];
_4.forEach(this._dojoList,function(_1e,i){
if(_1e.type!=_1c){
_1d.push(_1e);
}
});
this._dojoList=_1d;
}
},unregister:function(){
this._dojoList=[];
},refresh:function(){
var _1f=this._dojoList;
this.unregister();
_4.forEach(_1f,function(_20){
_20.coords=this._initCoordinates(_20.node);
},this);
this._dojoList=_1f;
},refreshByType:function(_21){
var _22=this._dojoList;
this.unregister();
_4.forEach(_22,function(_23){
if(_23.type==_21){
_23.coords=this._initCoordinates(_23.node);
}
},this);
this._dojoList=_22;
},_getHoverDojoArea:function(_24){
this._oldDojoArea=this._currentDojoArea;
this._currentDojoArea=null;
var x=_24.x;
var y=_24.y;
var _25=this._dojoList.length;
for(var i=0;i<_25;i++){
var _26=this._dojoList[i];
var _27=_26.coords;
if(_27.x<=x&&x<=_27.x1&&_27.y<=y&&y<=_27.y1){
this._currentDojoArea=_26;
break;
}
}
},onMouseMove:function(e){
var _28={"x":e.pageX,"y":e.pageY};
this._getHoverDojoArea(_28);
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
},isAccepted:function(_29,_2a){
return true;
},onDragEnter:function(e){
if(this._currentDojoArea.dojo){
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
_3.disconnect(this._dojoxManager._dragItem.handlers.pop());
_3.disconnect(this._dojoxManager._dragItem.item.events.pop());
_9.body().removeChild(this._dojoxManager._cover);
_9.body().removeChild(this._dojoxManager._cover2);
var _2b=this._dojoxManager._dragItem.item.node;
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.unsubscribeDnd();
}
_6.set(_2b,{"position":"relative","top":"0","left":"0"});
this._lazyManager.startDrag(e,_2b);
var _2c=_3.connect(this._lazyManager.manager,"overSource",this,function(){
_3.disconnect(_2c);
if(this._lazyManager.manager.canDropFlag){
this._dojoxManager._dropIndicator.node.style.display="none";
}
});
this.cancelHandler=_3.subscribe("/dnd/cancel",this,function(){
var _2d=this._dojoxManager._dragItem.item;
_2d.events=[_3.connect(_2d.handle,"onmousedown",_2d,"onMouseDown")];
_9.body().appendChild(this._dojoxManager._cover);
_9.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_2d.node);
var _2e=this._dojoxManager._areaList[this._dojoxManager._sourceIndexArea];
var _2f=this._dojoxManager._sourceDropIndex;
var _30=null;
if(_2f!=_2e.items.length&&_2f!=-1){
_30=_2e.items[this._dojoxManager._sourceDropIndex].item.node;
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
this.dropHandler=_3.subscribe("/dnd/drop/before",this,function(_31){
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
var _32=this._dojoxManager._dragItem.item;
this._dojoxManager._dragItem.item.events.push(_3.connect(_32.node.ownerDocument,"onmousemove",_32,"onMove"));
_9.body().appendChild(this._dojoxManager._cover);
_9.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_32.node);
var _33=_32.node.style;
_33.position="absolute";
_33.left=(_32.offsetDrag.l+e.pageX)+"px";
_33.top=(_32.offsetDrag.t+e.pageX)+"px";
_33.display="";
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
var _34=this._draggedNode.style;
_34.position="relative";
_34.left="0";
_34.top="0";
_34.width="auto";
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
return _a;
});
