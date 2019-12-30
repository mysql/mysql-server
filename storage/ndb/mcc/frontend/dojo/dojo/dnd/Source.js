/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Source",["../_base/array","../_base/connect","../_base/declare","../_base/kernel","../_base/lang","../dom-class","../dom-geometry","../mouse","../ready","../topic","./common","./Selector","./Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
if(!_4.isAsync){
_9(0,function(){
var _e=["dojo/dnd/AutoSource","dojo/dnd/Target"];
require(_e);
});
}
var _f=_3("dojo.dnd.Source",_c,{isSource:true,horizontal:false,copyOnly:false,selfCopy:false,selfAccept:true,skipForm:false,withHandles:false,autoSync:false,delay:0,accept:["text"],generateText:true,constructor:function(_10,_11){
_5.mixin(this,_5.mixin({},_11));
var _12=this.accept;
if(_12.length){
this.accept={};
for(var i=0;i<_12.length;++i){
this.accept[_12[i]]=1;
}
}
this.isDragging=false;
this.mouseDown=false;
this.targetAnchor=null;
this.targetBox=null;
this.before=true;
this._lastX=0;
this._lastY=0;
this.sourceState="";
if(this.isSource){
_6.add(this.node,"dojoDndSource");
}
this.targetState="";
if(this.accept){
_6.add(this.node,"dojoDndTarget");
}
if(this.horizontal){
_6.add(this.node,"dojoDndHorizontal");
}
this.topics=[_a.subscribe("/dnd/source/over",_5.hitch(this,"onDndSourceOver")),_a.subscribe("/dnd/start",_5.hitch(this,"onDndStart")),_a.subscribe("/dnd/drop",_5.hitch(this,"onDndDrop")),_a.subscribe("/dnd/cancel",_5.hitch(this,"onDndCancel"))];
},checkAcceptance:function(_13,_14){
if(this==_13){
return !this.copyOnly||this.selfAccept;
}
for(var i=0;i<_14.length;++i){
var _15=_13.getItem(_14[i].id).type;
var _16=false;
for(var j=0;j<_15.length;++j){
if(_15[j] in this.accept){
_16=true;
break;
}
}
if(!_16){
return false;
}
}
return true;
},copyState:function(_17,_18){
if(_17){
return true;
}
if(arguments.length<2){
_18=this==_d.manager().target;
}
if(_18){
if(this.copyOnly){
return this.selfCopy;
}
}else{
return this.copyOnly;
}
return false;
},destroy:function(){
_f.superclass.destroy.call(this);
_1.forEach(this.topics,function(t){
t.remove();
});
this.targetAnchor=null;
},onMouseMove:function(e){
if(this.isDragging&&this.targetState=="Disabled"){
return;
}
_f.superclass.onMouseMove.call(this,e);
var m=_d.manager();
if(!this.isDragging){
if(this.mouseDown&&this.isSource&&(Math.abs(e.pageX-this._lastX)>this.delay||Math.abs(e.pageY-this._lastY)>this.delay)){
var _19=this.getSelectedNodes();
if(_19.length){
m.startDrag(this,_19,this.copyState(_b.getCopyKeyState(e),true));
}
}
}
if(this.isDragging){
var _1a=false;
if(this.current){
if(!this.targetBox||this.targetAnchor!=this.current){
this.targetBox=_7.position(this.current,true);
}
if(this.horizontal){
_1a=(e.pageX-this.targetBox.x<this.targetBox.w/2)==_7.isBodyLtr(this.current.ownerDocument);
}else{
_1a=(e.pageY-this.targetBox.y)<(this.targetBox.h/2);
}
}
if(this.current!=this.targetAnchor||_1a!=this.before){
this._markTargetAnchor(_1a);
m.canDrop(!this.current||m.source!=this||!(this.current.id in this.selection));
}
}
},onMouseDown:function(e){
if(!this.mouseDown&&this._legalMouseDown(e)&&(!this.skipForm||!_b.isFormElement(e))){
this.mouseDown=true;
this._lastX=e.pageX;
this._lastY=e.pageY;
_f.superclass.onMouseDown.call(this,e);
}
},onMouseUp:function(e){
if(this.mouseDown){
this.mouseDown=false;
_f.superclass.onMouseUp.call(this,e);
}
},onDndSourceOver:function(_1b){
if(this!==_1b){
this.mouseDown=false;
if(this.targetAnchor){
this._unmarkTargetAnchor();
}
}else{
if(this.isDragging){
var m=_d.manager();
m.canDrop(this.targetState!="Disabled"&&(!this.current||m.source!=this||!(this.current.id in this.selection)));
}
}
},onDndStart:function(_1c,_1d,_1e){
if(this.autoSync){
this.sync();
}
if(this.isSource){
this._changeState("Source",this==_1c?(_1e?"Copied":"Moved"):"");
}
var _1f=this.accept&&this.checkAcceptance(_1c,_1d);
this._changeState("Target",_1f?"":"Disabled");
if(this==_1c){
_d.manager().overSource(this);
}
this.isDragging=true;
},onDndDrop:function(_20,_21,_22,_23){
if(this==_23){
this.onDrop(_20,_21,_22);
}
this.onDndCancel();
},onDndCancel:function(){
if(this.targetAnchor){
this._unmarkTargetAnchor();
this.targetAnchor=null;
}
this.before=true;
this.isDragging=false;
this.mouseDown=false;
this._changeState("Source","");
this._changeState("Target","");
},onDrop:function(_24,_25,_26){
if(this!=_24){
this.onDropExternal(_24,_25,_26);
}else{
this.onDropInternal(_25,_26);
}
},onDropExternal:function(_27,_28,_29){
var _2a=this._normalizedCreator;
if(this.creator){
this._normalizedCreator=function(_2b,_2c){
return _2a.call(this,_27.getItem(_2b.id).data,_2c);
};
}else{
if(_29){
this._normalizedCreator=function(_2d){
var t=_27.getItem(_2d.id);
var n=_2d.cloneNode(true);
n.id=_b.getUniqueId();
return {node:n,data:t.data,type:t.type};
};
}else{
this._normalizedCreator=function(_2e){
var t=_27.getItem(_2e.id);
_27.delItem(_2e.id);
return {node:_2e,data:t.data,type:t.type};
};
}
}
this.selectNone();
if(!_29&&!this.creator){
_27.selectNone();
}
this.insertNodes(true,_28,this.before,this.current);
if(!_29&&this.creator){
_27.deleteSelectedNodes();
}
this._normalizedCreator=_2a;
},onDropInternal:function(_2f,_30){
var _31=this._normalizedCreator;
if(this.current&&this.current.id in this.selection){
return;
}
if(_30){
if(this.creator){
this._normalizedCreator=function(_32,_33){
return _31.call(this,this.getItem(_32.id).data,_33);
};
}else{
this._normalizedCreator=function(_34){
var t=this.getItem(_34.id);
var n=_34.cloneNode(true);
n.id=_b.getUniqueId();
return {node:n,data:t.data,type:t.type};
};
}
}else{
if(!this.current){
return;
}
this._normalizedCreator=function(_35){
var t=this.getItem(_35.id);
return {node:_35,data:t.data,type:t.type};
};
}
this._removeSelection();
this.insertNodes(true,_2f,this.before,this.current);
this._normalizedCreator=_31;
},onDraggingOver:function(){
},onDraggingOut:function(){
},onOverEvent:function(){
_f.superclass.onOverEvent.call(this);
_d.manager().overSource(this);
if(this.isDragging&&this.targetState!="Disabled"){
this.onDraggingOver();
}
},onOutEvent:function(){
_f.superclass.onOutEvent.call(this);
_d.manager().outSource(this);
if(this.isDragging&&this.targetState!="Disabled"){
this.onDraggingOut();
}
},_markTargetAnchor:function(_36){
if(this.current==this.targetAnchor&&this.before==_36){
return;
}
if(this.targetAnchor){
this._removeItemClass(this.targetAnchor,this.before?"Before":"After");
}
this.targetAnchor=this.current;
this.targetBox=null;
this.before=_36;
if(this.targetAnchor){
this._addItemClass(this.targetAnchor,this.before?"Before":"After");
}
},_unmarkTargetAnchor:function(){
if(!this.targetAnchor){
return;
}
this._removeItemClass(this.targetAnchor,this.before?"Before":"After");
this.targetAnchor=null;
this.targetBox=null;
this.before=true;
},_markDndStatus:function(_37){
this._changeState("Source",_37?"Copied":"Moved");
},_legalMouseDown:function(e){
if(e.type!="touchstart"&&!_8.isLeft(e)){
return false;
}
if(!this.withHandles){
return true;
}
for(var _38=e.target;_38&&_38!==this.node;_38=_38.parentNode){
if(_6.contains(_38,"dojoDndHandle")){
return true;
}
if(_6.contains(_38,"dojoDndItem")||_6.contains(_38,"dojoDndIgnore")){
break;
}
}
return false;
}});
return _f;
});
