//>>built
define("dojox/mdnd/AreaManager",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/window","dojo/_base/array","dojo/_base/sniff","dojo/_base/lang","dojo/query","dojo/topic","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dijit/registry","dijit/_Widget","./Moveable"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var am=_2("dojox.mdnd.AreaManager",null,{autoRefresh:true,areaClass:"dojoxDndArea",dragHandleClass:"dojoxDragHandle",constructor:function(){
this._areaList=[];
this.resizeHandler=_3.connect(_1.global,"onresize",this,function(){
this._dropMode.updateAreas(this._areaList);
});
this._oldIndexArea=this._currentIndexArea=this._oldDropIndex=this._currentDropIndex=this._sourceIndexArea=this._sourceDropIndex=-1;
},init:function(){
this.registerByClass();
},registerByNode:function(_10,_11){
var _12=this._getIndexArea(_10);
if(_10&&_12==-1){
var _13=_10.getAttribute("accept");
var _14=(_13)?_13.split(/\s*,\s*/):["text"];
var obj={"node":_10,"items":[],"coords":{},"margin":null,"accept":_14,"initItems":false};
_5.forEach(this._getChildren(_10),function(_15){
this._setMarginArea(obj,_15);
obj.items.push(this._addMoveableItem(_15));
},this);
this._areaList=this._dropMode.addArea(this._areaList,obj);
if(!_11){
this._dropMode.updateAreas(this._areaList);
}
_3.publish("/dojox/mdnd/manager/register",[_10]);
}
},registerByClass:function(){
_8("."+this.areaClass).forEach(function(_16){
this.registerByNode(_16,true);
},this);
this._dropMode.updateAreas(this._areaList);
},unregister:function(_17){
var _18=this._getIndexArea(_17);
if(_18!=-1){
_5.forEach(this._areaList[_18].items,function(_19){
this._deleteMoveableItem(_19);
},this);
this._areaList.splice(_18,1);
this._dropMode.updateAreas(this._areaList);
return true;
}
return false;
},_addMoveableItem:function(_1a){
_1a.setAttribute("tabIndex","0");
var _1b=this._searchDragHandle(_1a);
var _1c=new _f({"handle":_1b,"skip":true},_1a);
_a.add(_1b||_1a,"dragHandle");
var _1d=_1a.getAttribute("dndType");
var _1e={"item":_1c,"type":_1d?_1d.split(/\s*,\s*/):["text"],"handlers":[_3.connect(_1c,"onDragStart",this,"onDragStart")]};
if(_d&&_d.byNode){
var _1f=_d.byNode(_1a);
if(_1f){
_1e.type=_1f.dndType?_1f.dndType.split(/\s*,\s*/):["text"];
_1e.handlers.push(_3.connect(_1f,"uninitialize",this,function(){
this.removeDragItem(_1a.parentNode,_1c.node);
}));
}
}
return _1e;
},_deleteMoveableItem:function(_20){
_5.forEach(_20.handlers,function(_21){
_3.disconnect(_21);
});
var _22=_20.item.node,_23=this._searchDragHandle(_22);
_a.remove(_23||_22,"dragHandle");
_20.item.destroy();
},_getIndexArea:function(_24){
if(_24){
for(var i=0;i<this._areaList.length;i++){
if(this._areaList[i].node===_24){
return i;
}
}
}
return -1;
},_searchDragHandle:function(_25){
if(_25){
var _26=this.dragHandleClass.split(" "),_27=_26.length,_28="";
_5.forEach(_26,function(css,i){
_28+="."+css;
if(i!=_27-1){
_28+=", ";
}
});
return _8(_28,_25)[0];
}
},addDragItem:function(_29,_2a,_2b,_2c){
var add=true;
if(!_2c){
add=_29&&_2a&&(_2a.parentNode===null||(_2a.parentNode&&_2a.parentNode.nodeType!==1));
}
if(add){
var _2d=this._getIndexArea(_29);
if(_2d!==-1){
var _2e=this._addMoveableItem(_2a),_2f=this._areaList[_2d].items;
if(0<=_2b&&_2b<_2f.length){
var _30=_2f.slice(0,_2b),_31=_2f.slice(_2b,_2f.length);
_30[_30.length]=_2e;
this._areaList[_2d].items=_30.concat(_31);
_29.insertBefore(_2a,_2f[_2b].item.node);
}else{
this._areaList[_2d].items.push(_2e);
_29.appendChild(_2a);
}
this._setMarginArea(this._areaList[_2d],_2a);
this._areaList[_2d].initItems=false;
return true;
}
}
return false;
},removeDragItem:function(_32,_33){
var _34=this._getIndexArea(_32);
if(_32&&_34!==-1){
var _35=this._areaList[_34].items;
for(var j=0;j<_35.length;j++){
if(_35[j].item.node===_33){
this._deleteMoveableItem(_35[j]);
_35.splice(j,1);
return _32.removeChild(_33);
}
}
}
return null;
},_getChildren:function(_36){
var _37=[];
_5.forEach(_36.childNodes,function(_38){
if(_38.nodeType==1){
if(_d&&_d.byNode){
var _39=_d.byNode(_38);
if(_39){
if(!_39.dragRestriction){
_37.push(_38);
}
}else{
_37.push(_38);
}
}else{
_37.push(_38);
}
}
});
return _37;
},_setMarginArea:function(_3a,_3b){
if(_3a&&_3a.margin===null&&_3b){
_3a.margin=_b.getMarginExtents(_3b);
}
},findCurrentIndexArea:function(_3c,_3d){
this._oldIndexArea=this._currentIndexArea;
this._currentIndexArea=this._dropMode.getTargetArea(this._areaList,_3c,this._currentIndexArea);
if(this._currentIndexArea!=this._oldIndexArea){
if(this._oldIndexArea!=-1){
this.onDragExit(_3c,_3d);
}
if(this._currentIndexArea!=-1){
this.onDragEnter(_3c,_3d);
}
}
return this._currentIndexArea;
},_isAccepted:function(_3e,_3f){
this._accept=false;
for(var i=0;i<_3f.length;++i){
for(var j=0;j<_3e.length;++j){
if(_3e[j]==_3f[i]){
this._accept=true;
break;
}
}
}
},onDragStart:function(_40,_41,_42){
if(this.autoRefresh){
this._dropMode.updateAreas(this._areaList);
}
var _43=(_6("webkit"))?_1.body():_1.body().parentNode;
if(!this._cover){
this._cover=_c.create("div",{"class":"dndCover"});
this._cover2=_7.clone(this._cover);
_a.add(this._cover2,"dndCover2");
}
var h=_43.scrollHeight+"px";
this._cover.style.height=this._cover2.style.height=h;
_1.body().appendChild(this._cover);
_1.body().appendChild(this._cover2);
this._dragStartHandler=_3.connect(_40.ownerDocument,"ondragstart",_1,"stopEvent");
this._sourceIndexArea=this._lastValidIndexArea=this._currentIndexArea=this._getIndexArea(_40.parentNode);
var _44=this._areaList[this._sourceIndexArea];
var _45=_44.items;
for(var i=0;i<_45.length;i++){
if(_45[i].item.node==_40){
this._dragItem=_45[i];
this._dragItem.handlers.push(_3.connect(this._dragItem.item,"onDrag",this,"onDrag"));
this._dragItem.handlers.push(_3.connect(this._dragItem.item,"onDragEnd",this,"onDrop"));
_45.splice(i,1);
this._currentDropIndex=this._sourceDropIndex=i;
break;
}
}
var _46=null;
if(this._sourceDropIndex!==_44.items.length){
_46=_44.items[this._sourceDropIndex].item.node;
}
if(_6("ie")>7){
this._eventsIE7=[_3.connect(this._cover,"onmouseover",_1,"stopEvent"),_3.connect(this._cover,"onmouseout",_1,"stopEvent"),_3.connect(this._cover,"onmouseenter",_1,"stopEvent"),_3.connect(this._cover,"onmouseleave",_1,"stopEvent")];
}
var s=_40.style;
s.left=_41.x+"px";
s.top=_41.y+"px";
if(s.position=="relative"||s.position==""){
s.position="absolute";
}
this._cover.appendChild(_40);
this._dropIndicator.place(_44.node,_46,_42);
_a.add(_40,"dragNode");
this._accept=true;
_3.publish("/dojox/mdnd/drag/start",[_40,_44,this._sourceDropIndex]);
},onDragEnter:function(_47,_48){
if(this._currentIndexArea===this._sourceIndexArea){
this._accept=true;
}else{
this._isAccepted(this._dragItem.type,this._areaList[this._currentIndexArea].accept);
}
},onDragExit:function(_49,_4a){
this._accept=false;
},onDrag:function(_4b,_4c,_4d,_4e){
var _4f=this._dropMode.getDragPoint(_4c,_4d,_4e);
this.findCurrentIndexArea(_4f,_4d);
if(this._currentIndexArea!==-1&&this._accept){
this.placeDropIndicator(_4f,_4d);
}
},placeDropIndicator:function(_50,_51){
this._oldDropIndex=this._currentDropIndex;
var _52=this._areaList[this._currentIndexArea];
if(!_52.initItems){
this._dropMode.initItems(_52);
}
this._currentDropIndex=this._dropMode.getDropIndex(_52,_50);
if(!(this._currentIndexArea===this._oldIndexArea&&this._oldDropIndex===this._currentDropIndex)){
this._placeDropIndicator(_51);
}
return this._currentDropIndex;
},_placeDropIndicator:function(_53){
var _54=this._areaList[this._lastValidIndexArea];
var _55=this._areaList[this._currentIndexArea];
this._dropMode.refreshItems(_54,this._oldDropIndex,_53,false);
var _56=null;
if(this._currentDropIndex!=-1){
_56=_55.items[this._currentDropIndex].item.node;
}
this._dropIndicator.place(_55.node,_56);
this._lastValidIndexArea=this._currentIndexArea;
this._dropMode.refreshItems(_55,this._currentDropIndex,_53,true);
},onDropCancel:function(){
if(!this._accept){
var _57=this._getIndexArea(this._dropIndicator.node.parentNode);
if(_57!=-1){
this._currentIndexArea=_57;
}else{
this._currentIndexArea=0;
}
}
},onDrop:function(_58){
this.onDropCancel();
var _59=this._areaList[this._currentIndexArea];
_a.remove(_58,"dragNode");
var _5a=_58.style;
_5a.position="relative";
_5a.left="0";
_5a.top="0";
_5a.width="auto";
if(_59.node==this._dropIndicator.node.parentNode){
_59.node.insertBefore(_58,this._dropIndicator.node);
}else{
_59.node.appendChild(_58);
this._currentDropIndex=_59.items.length;
}
var _5b=this._currentDropIndex;
if(_5b==-1){
_5b=_59.items.length;
}
var _5c=_59.items;
var _5d=_5c.slice(0,_5b);
var _5e=_5c.slice(_5b,_5c.length);
_5d[_5d.length]=this._dragItem;
_59.items=_5d.concat(_5e);
this._setMarginArea(_59,_58);
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
_3.publish("/dojox/mdnd/drop",[_58,_59,_5b]);
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
if(dojox.mdnd.autoScroll){
dojox.mdnd.autoScroll.destroy();
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
dojox.mdnd._areaManager=null;
dojox.mdnd.areaManager=function(){
if(!dojox.mdnd._areaManager){
dojox.mdnd._areaManager=new dojox.mdnd.AreaManager();
}
return dojox.mdnd._areaManager;
};
return am;
});
