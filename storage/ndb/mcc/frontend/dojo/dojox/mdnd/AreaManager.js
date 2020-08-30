//>>built
define("dojox/mdnd/AreaManager",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/window","dojo/_base/array","dojo/_base/sniff","dojo/_base/lang","dojo/query","dojo/topic","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dijit/registry","dijit/_Widget","./Moveable","dojox/mdnd/AutoScroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var am=_2("dojox.mdnd.AreaManager",null,{autoRefresh:true,areaClass:"dojoxDndArea",dragHandleClass:"dojoxDragHandle",constructor:function(){
this._areaList=[];
this.resizeHandler=_3.connect(_1.global,"onresize",this,function(){
this._dropMode.updateAreas(this._areaList);
});
this._oldIndexArea=this._currentIndexArea=this._oldDropIndex=this._currentDropIndex=this._sourceIndexArea=this._sourceDropIndex=-1;
},init:function(){
this.registerByClass();
},registerByNode:function(_11,_12){
var _13=this._getIndexArea(_11);
if(_11&&_13==-1){
var _14=_11.getAttribute("accept");
var _15=(_14)?_14.split(/\s*,\s*/):["text"];
var obj={"node":_11,"items":[],"coords":{},"margin":null,"accept":_15,"initItems":false};
_5.forEach(this._getChildren(_11),function(_16){
this._setMarginArea(obj,_16);
obj.items.push(this._addMoveableItem(_16));
},this);
this._areaList=this._dropMode.addArea(this._areaList,obj);
if(!_12){
this._dropMode.updateAreas(this._areaList);
}
_3.publish("/dojox/mdnd/manager/register",[_11]);
}
},registerByClass:function(){
_8("."+this.areaClass).forEach(function(_17){
this.registerByNode(_17,true);
},this);
this._dropMode.updateAreas(this._areaList);
},unregister:function(_18){
var _19=this._getIndexArea(_18);
if(_19!=-1){
_5.forEach(this._areaList[_19].items,function(_1a){
this._deleteMoveableItem(_1a);
},this);
this._areaList.splice(_19,1);
this._dropMode.updateAreas(this._areaList);
return true;
}
return false;
},_addMoveableItem:function(_1b){
_1b.setAttribute("tabIndex","0");
var _1c=this._searchDragHandle(_1b);
var _1d=new _f({"handle":_1c,"skip":true},_1b);
_a.add(_1c||_1b,"dragHandle");
var _1e=_1b.getAttribute("dndType");
var _1f={"item":_1d,"type":_1e?_1e.split(/\s*,\s*/):["text"],"handlers":[_3.connect(_1d,"onDragStart",this,"onDragStart")]};
if(_d&&_d.byNode){
var _20=_d.byNode(_1b);
if(_20){
_1f.type=_20.dndType?_20.dndType.split(/\s*,\s*/):["text"];
_1f.handlers.push(_3.connect(_20,"uninitialize",this,function(){
this.removeDragItem(_1b.parentNode,_1d.node);
}));
}
}
return _1f;
},_deleteMoveableItem:function(_21){
_5.forEach(_21.handlers,function(_22){
_3.disconnect(_22);
});
var _23=_21.item.node,_24=this._searchDragHandle(_23);
_a.remove(_24||_23,"dragHandle");
_21.item.destroy();
},_getIndexArea:function(_25){
if(_25){
for(var i=0;i<this._areaList.length;i++){
if(this._areaList[i].node===_25){
return i;
}
}
}
return -1;
},_searchDragHandle:function(_26){
if(_26){
var _27=this.dragHandleClass.split(" "),_28=_27.length,_29="";
_5.forEach(_27,function(css,i){
_29+="."+css;
if(i!=_28-1){
_29+=", ";
}
});
return _8(_29,_26)[0];
}
},addDragItem:function(_2a,_2b,_2c,_2d){
var add=true;
if(!_2d){
add=_2a&&_2b&&(_2b.parentNode===null||(_2b.parentNode&&_2b.parentNode.nodeType!==1));
}
if(add){
var _2e=this._getIndexArea(_2a);
if(_2e!==-1){
var _2f=this._addMoveableItem(_2b),_30=this._areaList[_2e].items;
if(0<=_2c&&_2c<_30.length){
var _31=_30.slice(0,_2c),_32=_30.slice(_2c,_30.length);
_31[_31.length]=_2f;
this._areaList[_2e].items=_31.concat(_32);
_2a.insertBefore(_2b,_30[_2c].item.node);
}else{
this._areaList[_2e].items.push(_2f);
_2a.appendChild(_2b);
}
this._setMarginArea(this._areaList[_2e],_2b);
this._areaList[_2e].initItems=false;
return true;
}
}
return false;
},removeDragItem:function(_33,_34){
var _35=this._getIndexArea(_33);
if(_33&&_35!==-1){
var _36=this._areaList[_35].items;
for(var j=0;j<_36.length;j++){
if(_36[j].item.node===_34){
this._deleteMoveableItem(_36[j]);
_36.splice(j,1);
return _33.removeChild(_34);
}
}
}
return null;
},_getChildren:function(_37){
var _38=[];
_5.forEach(_37.childNodes,function(_39){
if(_39.nodeType==1){
if(_d&&_d.byNode){
var _3a=_d.byNode(_39);
if(_3a){
if(!_3a.dragRestriction){
_38.push(_39);
}
}else{
_38.push(_39);
}
}else{
_38.push(_39);
}
}
});
return _38;
},_setMarginArea:function(_3b,_3c){
if(_3b&&_3b.margin===null&&_3c){
_3b.margin=_b.getMarginExtents(_3c);
}
},findCurrentIndexArea:function(_3d,_3e){
this._oldIndexArea=this._currentIndexArea;
this._currentIndexArea=this._dropMode.getTargetArea(this._areaList,_3d,this._currentIndexArea);
if(this._currentIndexArea!=this._oldIndexArea){
if(this._oldIndexArea!=-1){
this.onDragExit(_3d,_3e);
}
if(this._currentIndexArea!=-1){
this.onDragEnter(_3d,_3e);
}
}
return this._currentIndexArea;
},_isAccepted:function(_3f,_40){
this._accept=false;
for(var i=0;i<_40.length;++i){
for(var j=0;j<_3f.length;++j){
if(_3f[j]==_40[i]){
this._accept=true;
break;
}
}
}
},onDragStart:function(_41,_42,_43){
if(this.autoRefresh){
this._dropMode.updateAreas(this._areaList);
}
var _44=(_6("webkit"))?_1.body():_1.body().parentNode;
if(!this._cover){
this._cover=_c.create("div",{"class":"dndCover"});
this._cover2=_7.clone(this._cover);
_a.add(this._cover2,"dndCover2");
}
var h=_44.scrollHeight+"px";
this._cover.style.height=this._cover2.style.height=h;
_1.body().appendChild(this._cover);
_1.body().appendChild(this._cover2);
this._dragStartHandler=_3.connect(_41.ownerDocument,"ondragstart",_1,"stopEvent");
this._sourceIndexArea=this._lastValidIndexArea=this._currentIndexArea=this._getIndexArea(_41.parentNode);
var _45=this._areaList[this._sourceIndexArea];
var _46=_45.items;
for(var i=0;i<_46.length;i++){
if(_46[i].item.node==_41){
this._dragItem=_46[i];
this._dragItem.handlers.push(_3.connect(this._dragItem.item,"onDrag",this,"onDrag"));
this._dragItem.handlers.push(_3.connect(this._dragItem.item,"onDragEnd",this,"onDrop"));
_46.splice(i,1);
this._currentDropIndex=this._sourceDropIndex=i;
break;
}
}
var _47=null;
if(this._sourceDropIndex!==_45.items.length){
_47=_45.items[this._sourceDropIndex].item.node;
}
if(_6("ie")>7){
this._eventsIE7=[_3.connect(this._cover,"onmouseover",_1,"stopEvent"),_3.connect(this._cover,"onmouseout",_1,"stopEvent"),_3.connect(this._cover,"onmouseenter",_1,"stopEvent"),_3.connect(this._cover,"onmouseleave",_1,"stopEvent")];
}
var s=_41.style;
s.left=_42.x+"px";
s.top=_42.y+"px";
if(s.position=="relative"||s.position==""){
s.position="absolute";
}
this._cover.appendChild(_41);
this._dropIndicator.place(_45.node,_47,_43);
_a.add(_41,"dragNode");
this._accept=true;
_3.publish("/dojox/mdnd/drag/start",[_41,_45,this._sourceDropIndex]);
},onDragEnter:function(_48,_49){
if(this._currentIndexArea===this._sourceIndexArea){
this._accept=true;
}else{
this._isAccepted(this._dragItem.type,this._areaList[this._currentIndexArea].accept);
}
},onDragExit:function(_4a,_4b){
this._accept=false;
},onDrag:function(_4c,_4d,_4e,_4f){
var _50=this._dropMode.getDragPoint(_4d,_4e,_4f);
this.findCurrentIndexArea(_50,_4e);
if(this._currentIndexArea!==-1&&this._accept){
this.placeDropIndicator(_50,_4e);
}
},placeDropIndicator:function(_51,_52){
this._oldDropIndex=this._currentDropIndex;
var _53=this._areaList[this._currentIndexArea];
if(!_53.initItems){
this._dropMode.initItems(_53);
}
this._currentDropIndex=this._dropMode.getDropIndex(_53,_51);
if(!(this._currentIndexArea===this._oldIndexArea&&this._oldDropIndex===this._currentDropIndex)){
this._placeDropIndicator(_52);
}
return this._currentDropIndex;
},_placeDropIndicator:function(_54){
var _55=this._areaList[this._lastValidIndexArea];
var _56=this._areaList[this._currentIndexArea];
this._dropMode.refreshItems(_55,this._oldDropIndex,_54,false);
var _57=null;
if(this._currentDropIndex!=-1){
_57=_56.items[this._currentDropIndex].item.node;
}
this._dropIndicator.place(_56.node,_57);
this._lastValidIndexArea=this._currentIndexArea;
this._dropMode.refreshItems(_56,this._currentDropIndex,_54,true);
},onDropCancel:function(){
if(!this._accept){
var _58=this._getIndexArea(this._dropIndicator.node.parentNode);
if(_58!=-1){
this._currentIndexArea=_58;
}else{
this._currentIndexArea=0;
}
}
},onDrop:function(_59){
this.onDropCancel();
var _5a=this._areaList[this._currentIndexArea];
_a.remove(_59,"dragNode");
var _5b=_59.style;
_5b.position="relative";
_5b.left="0";
_5b.top="0";
_5b.width="auto";
if(_5a.node==this._dropIndicator.node.parentNode){
_5a.node.insertBefore(_59,this._dropIndicator.node);
}else{
_5a.node.appendChild(_59);
this._currentDropIndex=_5a.items.length;
}
var _5c=this._currentDropIndex;
if(_5c==-1){
_5c=_5a.items.length;
}
var _5d=_5a.items;
var _5e=_5d.slice(0,_5c);
var _5f=_5d.slice(_5c,_5d.length);
_5e[_5e.length]=this._dragItem;
_5a.items=_5e.concat(_5f);
this._setMarginArea(_5a,_59);
_5.forEach(this._areaList,function(obj){
obj.initItems=false;
});
_3.disconnect(this._dragItem.handlers.pop());
_3.disconnect(this._dragItem.handlers.pop());
this._resetAfterDrop();
if(this._cover){
_1.body().removeChild(this._cover);
_1.body().removeChild(this._cover2);
}
_3.publish("/dojox/mdnd/drop",[_59,_5a,_5c]);
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
_3.disconnect(this._dragStartHandler);
}
if(_6("ie")>7){
_5.forEach(this._eventsIE7,_3.disconnect);
}
},destroy:function(){
while(this._areaList.length>0){
if(!this.unregister(this._areaList[0].node)){
throw new Error("Error while destroying AreaManager");
}
}
_3.disconnect(this.resizeHandler);
this._dropIndicator.destroy();
this._dropMode.destroy();
if(_10.autoScroll){
_10.autoScroll.destroy();
}
if(this.refreshListener){
_3.unsubscribe(this.refreshListener);
}
if(this._cover){
_c.destroy(this._cover);
_c.destroy(this._cover2);
delete this._cover;
delete this._cover2;
}
}});
if(_e){
_7.extend(_e,{dndType:"text"});
}
am._areaManager=null;
am.areaManager=function(){
if(!am._areaManager){
am._areaManager=new am();
}
return am._areaManager;
};
return am;
});
