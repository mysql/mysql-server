//>>built
define("dojox/mdnd/AreaManager",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/window","dojo/_base/array","dojo/query","dojo/_base/html","./Moveable"],function(_1){
var am=_1.declare("dojox.mdnd.AreaManager",null,{autoRefresh:true,areaClass:"dojoxDndArea",dragHandleClass:"dojoxDragHandle",constructor:function(){
this._areaList=[];
this.resizeHandler=_1.connect(_1.global,"onresize",this,function(){
this._dropMode.updateAreas(this._areaList);
});
this._oldIndexArea=this._currentIndexArea=this._oldDropIndex=this._currentDropIndex=this._sourceIndexArea=this._sourceDropIndex=-1;
},init:function(){
this.registerByClass();
},registerByNode:function(_2,_3){
var _4=this._getIndexArea(_2);
if(_2&&_4==-1){
var _5=_2.getAttribute("accept");
var _6=(_5)?_5.split(/\s*,\s*/):["text"];
var _7={"node":_2,"items":[],"coords":{},"margin":null,"accept":_6,"initItems":false};
_1.forEach(this._getChildren(_2),function(_8){
this._setMarginArea(_7,_8);
_7.items.push(this._addMoveableItem(_8));
},this);
this._areaList=this._dropMode.addArea(this._areaList,_7);
if(!_3){
this._dropMode.updateAreas(this._areaList);
}
_1.publish("/dojox/mdnd/manager/register",[_2]);
}
},registerByClass:function(){
_1.query("."+this.areaClass).forEach(function(_9){
this.registerByNode(_9,true);
},this);
this._dropMode.updateAreas(this._areaList);
},unregister:function(_a){
var _b=this._getIndexArea(_a);
if(_b!=-1){
_1.forEach(this._areaList[_b].items,function(_c){
this._deleteMoveableItem(_c);
},this);
this._areaList.splice(_b,1);
this._dropMode.updateAreas(this._areaList);
return true;
}
return false;
},_addMoveableItem:function(_d){
_d.setAttribute("tabIndex","0");
var _e=this._searchDragHandle(_d);
var _f=new dojox.mdnd.Moveable({"handle":_e,"skip":true},_d);
_1.addClass(_e||_d,"dragHandle");
var _10=_d.getAttribute("dndType");
var _11={"item":_f,"type":_10?_10.split(/\s*,\s*/):["text"],"handlers":[_1.connect(_f,"onDragStart",this,"onDragStart")]};
if(dijit&&dijit.byNode){
var _12=dijit.byNode(_d);
if(_12){
_11.type=_12.dndType?_12.dndType.split(/\s*,\s*/):["text"];
_11.handlers.push(_1.connect(_12,"uninitialize",this,function(){
this.removeDragItem(_d.parentNode,_f.node);
}));
}
}
return _11;
},_deleteMoveableItem:function(_13){
_1.forEach(_13.handlers,function(_14){
_1.disconnect(_14);
});
var _15=_13.item.node,_16=this._searchDragHandle(_15);
_1.removeClass(_16||_15,"dragHandle");
_13.item.destroy();
},_getIndexArea:function(_17){
if(_17){
for(var i=0;i<this._areaList.length;i++){
if(this._areaList[i].node===_17){
return i;
}
}
}
return -1;
},_searchDragHandle:function(_18){
if(_18){
var _19=this.dragHandleClass.split(" "),_1a=_19.length,_1b="";
_1.forEach(_19,function(css,i){
_1b+="."+css;
if(i!=_1a-1){
_1b+=", ";
}
});
return _1.query(_1b,_18)[0];
}
},addDragItem:function(_1c,_1d,_1e,_1f){
var add=true;
if(!_1f){
add=_1c&&_1d&&(_1d.parentNode===null||(_1d.parentNode&&_1d.parentNode.nodeType!==1));
}
if(add){
var _20=this._getIndexArea(_1c);
if(_20!==-1){
var _21=this._addMoveableItem(_1d),_22=this._areaList[_20].items;
if(0<=_1e&&_1e<_22.length){
var _23=_22.slice(0,_1e),_24=_22.slice(_1e,_22.length);
_23[_23.length]=_21;
this._areaList[_20].items=_23.concat(_24);
_1c.insertBefore(_1d,_22[_1e].item.node);
}else{
this._areaList[_20].items.push(_21);
_1c.appendChild(_1d);
}
this._setMarginArea(this._areaList[_20],_1d);
this._areaList[_20].initItems=false;
return true;
}
}
return false;
},removeDragItem:function(_25,_26){
var _27=this._getIndexArea(_25);
if(_25&&_27!==-1){
var _28=this._areaList[_27].items;
for(var j=0;j<_28.length;j++){
if(_28[j].item.node===_26){
this._deleteMoveableItem(_28[j]);
_28.splice(j,1);
return _25.removeChild(_26);
}
}
}
return null;
},_getChildren:function(_29){
var _2a=[];
_1.forEach(_29.childNodes,function(_2b){
if(_2b.nodeType==1){
if(dijit&&dijit.byNode){
var _2c=dijit.byNode(_2b);
if(_2c){
if(!_2c.dragRestriction){
_2a.push(_2b);
}
}else{
_2a.push(_2b);
}
}else{
_2a.push(_2b);
}
}
});
return _2a;
},_setMarginArea:function(_2d,_2e){
if(_2d&&_2d.margin===null&&_2e){
_2d.margin=_1._getMarginExtents(_2e);
}
},findCurrentIndexArea:function(_2f,_30){
this._oldIndexArea=this._currentIndexArea;
this._currentIndexArea=this._dropMode.getTargetArea(this._areaList,_2f,this._currentIndexArea);
if(this._currentIndexArea!=this._oldIndexArea){
if(this._oldIndexArea!=-1){
this.onDragExit(_2f,_30);
}
if(this._currentIndexArea!=-1){
this.onDragEnter(_2f,_30);
}
}
return this._currentIndexArea;
},_isAccepted:function(_31,_32){
this._accept=false;
for(var i=0;i<_32.length;++i){
for(var j=0;j<_31.length;++j){
if(_31[j]==_32[i]){
this._accept=true;
break;
}
}
}
},onDragStart:function(_33,_34,_35){
if(this.autoRefresh){
this._dropMode.updateAreas(this._areaList);
}
var _36=(_1.isWebKit)?_1.body():_1.body().parentNode;
if(!this._cover){
this._cover=_1.create("div",{"class":"dndCover"});
this._cover2=_1.clone(this._cover);
_1.addClass(this._cover2,"dndCover2");
}
var h=_36.scrollHeight+"px";
this._cover.style.height=this._cover2.style.height=h;
_1.body().appendChild(this._cover);
_1.body().appendChild(this._cover2);
this._dragStartHandler=_1.connect(_33.ownerDocument,"ondragstart",_1,"stopEvent");
this._sourceIndexArea=this._lastValidIndexArea=this._currentIndexArea=this._getIndexArea(_33.parentNode);
var _37=this._areaList[this._sourceIndexArea];
var _38=_37.items;
for(var i=0;i<_38.length;i++){
if(_38[i].item.node==_33){
this._dragItem=_38[i];
this._dragItem.handlers.push(_1.connect(this._dragItem.item,"onDrag",this,"onDrag"));
this._dragItem.handlers.push(_1.connect(this._dragItem.item,"onDragEnd",this,"onDrop"));
_38.splice(i,1);
this._currentDropIndex=this._sourceDropIndex=i;
break;
}
}
var _39=null;
if(this._sourceDropIndex!==_37.items.length){
_39=_37.items[this._sourceDropIndex].item.node;
}
if(_1.isIE>7){
this._eventsIE7=[_1.connect(this._cover,"onmouseover",_1,"stopEvent"),_1.connect(this._cover,"onmouseout",_1,"stopEvent"),_1.connect(this._cover,"onmouseenter",_1,"stopEvent"),_1.connect(this._cover,"onmouseleave",_1,"stopEvent")];
}
var s=_33.style;
s.left=_34.x+"px";
s.top=_34.y+"px";
if(s.position=="relative"||s.position==""){
s.position="absolute";
}
this._cover.appendChild(_33);
this._dropIndicator.place(_37.node,_39,_35);
_1.addClass(_33,"dragNode");
this._accept=true;
_1.publish("/dojox/mdnd/drag/start",[_33,_37,this._sourceDropIndex]);
},onDragEnter:function(_3a,_3b){
if(this._currentIndexArea===this._sourceIndexArea){
this._accept=true;
}else{
this._isAccepted(this._dragItem.type,this._areaList[this._currentIndexArea].accept);
}
},onDragExit:function(_3c,_3d){
this._accept=false;
},onDrag:function(_3e,_3f,_40,_41){
var _42=this._dropMode.getDragPoint(_3f,_40,_41);
this.findCurrentIndexArea(_42,_40);
if(this._currentIndexArea!==-1&&this._accept){
this.placeDropIndicator(_42,_40);
}
},placeDropIndicator:function(_43,_44){
this._oldDropIndex=this._currentDropIndex;
var _45=this._areaList[this._currentIndexArea];
if(!_45.initItems){
this._dropMode.initItems(_45);
}
this._currentDropIndex=this._dropMode.getDropIndex(_45,_43);
if(!(this._currentIndexArea===this._oldIndexArea&&this._oldDropIndex===this._currentDropIndex)){
this._placeDropIndicator(_44);
}
return this._currentDropIndex;
},_placeDropIndicator:function(_46){
var _47=this._areaList[this._lastValidIndexArea];
var _48=this._areaList[this._currentIndexArea];
this._dropMode.refreshItems(_47,this._oldDropIndex,_46,false);
var _49=null;
if(this._currentDropIndex!=-1){
_49=_48.items[this._currentDropIndex].item.node;
}
this._dropIndicator.place(_48.node,_49);
this._lastValidIndexArea=this._currentIndexArea;
this._dropMode.refreshItems(_48,this._currentDropIndex,_46,true);
},onDropCancel:function(){
if(!this._accept){
var _4a=this._getIndexArea(this._dropIndicator.node.parentNode);
if(_4a!=-1){
this._currentIndexArea=_4a;
}else{
this._currentIndexArea=0;
}
}
},onDrop:function(_4b){
this.onDropCancel();
var _4c=this._areaList[this._currentIndexArea];
_1.removeClass(_4b,"dragNode");
var _4d=_4b.style;
_4d.position="relative";
_4d.left="0";
_4d.top="0";
_4d.width="auto";
if(_4c.node==this._dropIndicator.node.parentNode){
_4c.node.insertBefore(_4b,this._dropIndicator.node);
}else{
_4c.node.appendChild(_4b);
this._currentDropIndex=_4c.items.length;
}
var _4e=this._currentDropIndex;
if(_4e==-1){
_4e=_4c.items.length;
}
var _4f=_4c.items;
var _50=_4f.slice(0,_4e);
var _51=_4f.slice(_4e,_4f.length);
_50[_50.length]=this._dragItem;
_4c.items=_50.concat(_51);
this._setMarginArea(_4c,_4b);
_1.forEach(this._areaList,function(obj){
obj.initItems=false;
});
_1.disconnect(this._dragItem.handlers.pop());
_1.disconnect(this._dragItem.handlers.pop());
this._resetAfterDrop();
if(this._cover){
_1.body().removeChild(this._cover);
_1.body().removeChild(this._cover2);
}
_1.publish("/dojox/mdnd/drop",[_4b,_4c,_4e]);
},_resetAfterDrop:function(){
this._accept=false;
this._dragItem=null;
this._currentDropIndex=-1;
this._currentIndexArea=-1;
this._oldDropIndex=-1;
this._sourceIndexArea=-1;
this._sourceDropIndex=-1;
this._dropIndicator.remove();
if(this._dragStartHandler){
_1.disconnect(this._dragStartHandler);
}
if(_1.isIE>7){
_1.forEach(this._eventsIE7,_1.disconnect);
}
},destroy:function(){
while(this._areaList.length>0){
if(!this.unregister(this._areaList[0].node)){
throw new Error("Error while destroying AreaManager");
}
}
_1.disconnect(this.resizeHandler);
this._dropIndicator.destroy();
this._dropMode.destroy();
if(dojox.mdnd.autoScroll){
dojox.mdnd.autoScroll.destroy();
}
if(this.refreshListener){
_1.unsubscribe(this.refreshListener);
}
if(this._cover){
_1._destroyElement(this._cover);
_1._destroyElement(this._cover2);
delete this._cover;
delete this._cover2;
}
}});
if(dijit&&dijit._Widget){
_1.extend(dijit._Widget,{dndType:"text"});
}
dojox.mdnd._areaManager=null;
dojox.mdnd.areaManager=function(){
if(!dojox.mdnd._areaManager){
dojox.mdnd._areaManager=new dojox.mdnd.AreaManager();
}
return dojox.mdnd._areaManager;
};
return am;
});
