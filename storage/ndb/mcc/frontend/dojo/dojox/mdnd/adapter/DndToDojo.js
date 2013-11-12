//>>built
define("dojox/mdnd/adapter/DndToDojo",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","dojo/_base/connect","dojo/_base/window","dojo/_base/array","dojox/mdnd/PureSource","dojox/mdnd/LazyManager"],function(_1){
var _2=_1.declare("dojox.mdnd.adapter.DndToDojo",null,{_dojoList:null,_currentDojoArea:null,_dojoxManager:null,_dragStartHandler:null,_dropHandler:null,_moveHandler:null,_moveUpHandler:null,_draggedNode:null,constructor:function(){
this._dojoList=[];
this._currentDojoArea=null;
this._dojoxManager=dojox.mdnd.areaManager();
this._dragStartHandler=_1.subscribe("/dojox/mdnd/drag/start",this,function(_3,_4,_5){
this._draggedNode=_3;
this._moveHandler=_1.connect(_1.doc,"onmousemove",this,"onMouseMove");
});
this._dropHandler=_1.subscribe("/dojox/mdnd/drop",this,function(_6,_7,_8){
if(this._currentDojoArea){
_1.publish("/dojox/mdnd/adapter/dndToDojo/cancel",[this._currentDojoArea.node,this._currentDojoArea.type,this._draggedNode,this.accept]);
}
this._draggedNode=null;
this._currentDojoArea=null;
_1.disconnect(this._moveHandler);
});
},_getIndexDojoArea:function(_9){
if(_9){
for(var i=0,l=this._dojoList.length;i<l;i++){
if(this._dojoList[i].node===_9){
return i;
}
}
}
return -1;
},_initCoordinates:function(_a){
if(_a){
var _b=_1.position(_a,true),_c={};
_c.x=_b.x;
_c.y=_b.y;
_c.x1=_b.x+_b.w;
_c.y1=_b.y+_b.h;
return _c;
}
return null;
},register:function(_d,_e,_f){
if(this._getIndexDojoArea(_d)==-1){
var _10=this._initCoordinates(_d),_11={"node":_d,"type":_e,"dojo":(_f)?_f:false,"coords":_10};
this._dojoList.push(_11);
if(_f&&!this._lazyManager){
this._lazyManager=new dojox.mdnd.LazyManager();
}
}
},unregisterByNode:function(_12){
var _13=this._getIndexDojoArea(_12);
if(_13!=-1){
this._dojoList.splice(_13,1);
}
},unregisterByType:function(_14){
if(_14){
var _15=[];
_1.forEach(this._dojoList,function(_16,i){
if(_16.type!=_14){
_15.push(_16);
}
});
this._dojoList=_15;
}
},unregister:function(){
this._dojoList=[];
},refresh:function(){
var _17=this._dojoList;
this.unregister();
_1.forEach(_17,function(_18){
_18.coords=this._initCoordinates(_18.node);
},this);
this._dojoList=_17;
},refreshByType:function(_19){
var _1a=this._dojoList;
this.unregister();
_1.forEach(_1a,function(_1b){
if(_1b.type==_19){
_1b.coords=this._initCoordinates(_1b.node);
}
},this);
this._dojoList=_1a;
},_getHoverDojoArea:function(_1c){
this._oldDojoArea=this._currentDojoArea;
this._currentDojoArea=null;
var x=_1c.x;
var y=_1c.y;
var _1d=this._dojoList.length;
for(var i=0;i<_1d;i++){
var _1e=this._dojoList[i];
var _1f=_1e.coords;
if(_1f.x<=x&&x<=_1f.x1&&_1f.y<=y&&y<=_1f.y1){
this._currentDojoArea=_1e;
break;
}
}
},onMouseMove:function(e){
var _20={"x":e.pageX,"y":e.pageY};
this._getHoverDojoArea(_20);
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
},isAccepted:function(_21,_22){
return true;
},onDragEnter:function(e){
if(this._currentDojoArea.dojo){
_1.disconnect(this._dojoxManager._dragItem.handlers.pop());
_1.disconnect(this._dojoxManager._dragItem.handlers.pop());
_1.disconnect(this._dojoxManager._dragItem.item.events.pop());
_1.body().removeChild(this._dojoxManager._cover);
_1.body().removeChild(this._dojoxManager._cover2);
var _23=this._dojoxManager._dragItem.item.node;
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.unsubscribeDnd();
}
_1.style(_23,{"position":"relative","top":"0","left":"0"});
this._lazyManager.startDrag(e,_23);
var _24=_1.connect(this._lazyManager.manager,"overSource",this,function(){
_1.disconnect(_24);
if(this._lazyManager.manager.canDropFlag){
this._dojoxManager._dropIndicator.node.style.display="none";
}
});
this.cancelHandler=_1.subscribe("/dnd/cancel",this,function(){
var _25=this._dojoxManager._dragItem.item;
_25.events=[_1.connect(_25.handle,"onmousedown",_25,"onMouseDown")];
_1.body().appendChild(this._dojoxManager._cover);
_1.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_25.node);
var _26=this._dojoxManager._areaList[this._dojoxManager._sourceIndexArea];
var _27=this._dojoxManager._sourceDropIndex;
var _28=null;
if(_27!=_26.items.length&&_27!=-1){
_28=_26.items[this._dojoxManager._sourceDropIndex].item.node;
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display=="";
}
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._draggedNode.style.display="";
this._dojoxManager.onDrop(this._draggedNode);
_1.unsubscribe(this.cancelHandler);
_1.unsubscribe(this.dropHandler);
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.subscribeDnd();
}
});
this.dropHandler=_1.subscribe("/dnd/drop/before",this,function(_29){
_1.unsubscribe(this.cancelHandler);
_1.unsubscribe(this.dropHandler);
this.onDrop();
});
}else{
this.accept=this.isAccepted(this._dojoxManager._dragItem.item.node,this._currentDojoArea);
if(this.accept){
_1.disconnect(this._dojoxManager._dragItem.handlers.pop());
_1.disconnect(this._dojoxManager._dragItem.handlers.pop());
this._dojoxManager._dropIndicator.node.style.display="none";
if(!this._moveUpHandler){
this._moveUpHandler=_1.connect(_1.doc,"onmouseup",this,"onDrop");
}
}
}
_1.publish("/dojox/mdnd/adapter/dndToDojo/over",[this._currentDojoArea.node,this._currentDojoArea.type,this._draggedNode,this.accept]);
},onDragExit:function(e){
if(this._oldDojoArea.dojo){
_1.unsubscribe(this.cancelHandler);
_1.unsubscribe(this.dropHandler);
var _2a=this._dojoxManager._dragItem.item;
this._dojoxManager._dragItem.item.events.push(_1.connect(_2a.node.ownerDocument,"onmousemove",_2a,"onMove"));
_1.body().appendChild(this._dojoxManager._cover);
_1.body().appendChild(this._dojoxManager._cover2);
this._dojoxManager._cover.appendChild(_2a.node);
var _2b=_2a.node.style;
_2b.position="absolute";
_2b.left=(_2a.offsetDrag.l+e.pageX)+"px";
_2b.top=(_2a.offsetDrag.t+e.pageX)+"px";
_2b.display="";
this._lazyManager.cancelDrag();
if(dojox.mdnd.adapter._dndFromDojo){
dojox.mdnd.adapter._dndFromDojo.subscribeDnd();
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display="";
}
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._dojoxManager._dragItem.item.onMove(e);
}else{
if(this.accept){
if(this._moveUpHandler){
_1.disconnect(this._moveUpHandler);
this._moveUpHandler=null;
}
if(this._dojoxManager._dropIndicator.node.style.display=="none"){
this._dojoxManager._dropIndicator.node.style.display="";
}
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDrag",this._dojoxManager,"onDrag"));
this._dojoxManager._dragItem.handlers.push(_1.connect(this._dojoxManager._dragItem.item,"onDragEnd",this._dojoxManager,"onDrop"));
this._dojoxManager._dragItem.item.onMove(e);
}
}
_1.publish("/dojox/mdnd/adapter/dndToDojo/out",[this._oldDojoArea.node,this._oldDojoArea.type,this._draggedNode,this.accept]);
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
_1.body().removeChild(this._dojoxManager._cover);
_1.body().removeChild(this._dojoxManager._cover2);
}
if(this._draggedNode.parentNode==this._dojoxManager._cover){
this._dojoxManager._cover.removeChild(this._draggedNode);
}
_1.disconnect(this._moveHandler);
_1.disconnect(this._moveUpHandler);
this._moveHandler=this._moveUpHandler=null;
_1.publish("/dojox/mdnd/adapter/dndToDojo/drop",[this._draggedNode,this._currentDojoArea.node,this._currentDojoArea.type]);
_1.removeClass(this._draggedNode,"dragNode");
var _2c=this._draggedNode.style;
_2c.position="relative";
_2c.left="0";
_2c.top="0";
_2c.width="auto";
_1.forEach(this._dojoxManager._dragItem.handlers,_1.disconnect);
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
return _2;
});
