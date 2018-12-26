/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Source",["../main","./Selector","./Manager"],function(_1,_2,_3){
if(!_1.isAsync){
_1.ready(0,function(){
var _4=["dojo/dnd/AutoSource","dojo/dnd/Target"];
require(_4);
});
}
return _1.declare("dojo.dnd.Source",_2,{isSource:true,horizontal:false,copyOnly:false,selfCopy:false,selfAccept:true,skipForm:false,withHandles:false,autoSync:false,delay:0,accept:["text"],generateText:true,constructor:function(_5,_6){
_1.mixin(this,_1.mixin({},_6));
var _7=this.accept;
if(_7.length){
this.accept={};
for(var i=0;i<_7.length;++i){
this.accept[_7[i]]=1;
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
_1.addClass(this.node,"dojoDndSource");
}
this.targetState="";
if(this.accept){
_1.addClass(this.node,"dojoDndTarget");
}
if(this.horizontal){
_1.addClass(this.node,"dojoDndHorizontal");
}
this.topics=[_1.subscribe("/dnd/source/over",this,"onDndSourceOver"),_1.subscribe("/dnd/start",this,"onDndStart"),_1.subscribe("/dnd/drop",this,"onDndDrop"),_1.subscribe("/dnd/cancel",this,"onDndCancel")];
},checkAcceptance:function(_8,_9){
if(this==_8){
return !this.copyOnly||this.selfAccept;
}
for(var i=0;i<_9.length;++i){
var _a=_8.getItem(_9[i].id).type;
var _b=false;
for(var j=0;j<_a.length;++j){
if(_a[j] in this.accept){
_b=true;
break;
}
}
if(!_b){
return false;
}
}
return true;
},copyState:function(_c,_d){
if(_c){
return true;
}
if(arguments.length<2){
_d=this==_3.manager().target;
}
if(_d){
if(this.copyOnly){
return this.selfCopy;
}
}else{
return this.copyOnly;
}
return false;
},destroy:function(){
_1.dnd.Source.superclass.destroy.call(this);
_1.forEach(this.topics,_1.unsubscribe);
this.targetAnchor=null;
},onMouseMove:function(e){
if(this.isDragging&&this.targetState=="Disabled"){
return;
}
_1.dnd.Source.superclass.onMouseMove.call(this,e);
var m=_3.manager();
if(!this.isDragging){
if(this.mouseDown&&this.isSource&&(Math.abs(e.pageX-this._lastX)>this.delay||Math.abs(e.pageY-this._lastY)>this.delay)){
var _e=this.getSelectedNodes();
if(_e.length){
m.startDrag(this,_e,this.copyState(_1.isCopyKey(e),true));
}
}
}
if(this.isDragging){
var _f=false;
if(this.current){
if(!this.targetBox||this.targetAnchor!=this.current){
this.targetBox=_1.position(this.current,true);
}
if(this.horizontal){
_f=(e.pageX-this.targetBox.x)<(this.targetBox.w/2);
}else{
_f=(e.pageY-this.targetBox.y)<(this.targetBox.h/2);
}
}
if(this.current!=this.targetAnchor||_f!=this.before){
this._markTargetAnchor(_f);
m.canDrop(!this.current||m.source!=this||!(this.current.id in this.selection));
}
}
},onMouseDown:function(e){
if(!this.mouseDown&&this._legalMouseDown(e)&&(!this.skipForm||!_1.dnd.isFormElement(e))){
this.mouseDown=true;
this._lastX=e.pageX;
this._lastY=e.pageY;
_1.dnd.Source.superclass.onMouseDown.call(this,e);
}
},onMouseUp:function(e){
if(this.mouseDown){
this.mouseDown=false;
_1.dnd.Source.superclass.onMouseUp.call(this,e);
}
},onDndSourceOver:function(_10){
if(this!=_10){
this.mouseDown=false;
if(this.targetAnchor){
this._unmarkTargetAnchor();
}
}else{
if(this.isDragging){
var m=_3.manager();
m.canDrop(this.targetState!="Disabled"&&(!this.current||m.source!=this||!(this.current.id in this.selection)));
}
}
},onDndStart:function(_11,_12,_13){
if(this.autoSync){
this.sync();
}
if(this.isSource){
this._changeState("Source",this==_11?(_13?"Copied":"Moved"):"");
}
var _14=this.accept&&this.checkAcceptance(_11,_12);
this._changeState("Target",_14?"":"Disabled");
if(this==_11){
_3.manager().overSource(this);
}
this.isDragging=true;
},onDndDrop:function(_15,_16,_17,_18){
if(this==_18){
this.onDrop(_15,_16,_17);
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
},onDrop:function(_19,_1a,_1b){
if(this!=_19){
this.onDropExternal(_19,_1a,_1b);
}else{
this.onDropInternal(_1a,_1b);
}
},onDropExternal:function(_1c,_1d,_1e){
var _1f=this._normalizedCreator;
if(this.creator){
this._normalizedCreator=function(_20,_21){
return _1f.call(this,_1c.getItem(_20.id).data,_21);
};
}else{
if(_1e){
this._normalizedCreator=function(_22,_23){
var t=_1c.getItem(_22.id);
var n=_22.cloneNode(true);
n.id=_1.dnd.getUniqueId();
return {node:n,data:t.data,type:t.type};
};
}else{
this._normalizedCreator=function(_24,_25){
var t=_1c.getItem(_24.id);
_1c.delItem(_24.id);
return {node:_24,data:t.data,type:t.type};
};
}
}
this.selectNone();
if(!_1e&&!this.creator){
_1c.selectNone();
}
this.insertNodes(true,_1d,this.before,this.current);
if(!_1e&&this.creator){
_1c.deleteSelectedNodes();
}
this._normalizedCreator=_1f;
},onDropInternal:function(_26,_27){
var _28=this._normalizedCreator;
if(this.current&&this.current.id in this.selection){
return;
}
if(_27){
if(this.creator){
this._normalizedCreator=function(_29,_2a){
return _28.call(this,this.getItem(_29.id).data,_2a);
};
}else{
this._normalizedCreator=function(_2b,_2c){
var t=this.getItem(_2b.id);
var n=_2b.cloneNode(true);
n.id=_1.dnd.getUniqueId();
return {node:n,data:t.data,type:t.type};
};
}
}else{
if(!this.current){
return;
}
this._normalizedCreator=function(_2d,_2e){
var t=this.getItem(_2d.id);
return {node:_2d,data:t.data,type:t.type};
};
}
this._removeSelection();
this.insertNodes(true,_26,this.before,this.current);
this._normalizedCreator=_28;
},onDraggingOver:function(){
},onDraggingOut:function(){
},onOverEvent:function(){
_1.dnd.Source.superclass.onOverEvent.call(this);
_3.manager().overSource(this);
if(this.isDragging&&this.targetState!="Disabled"){
this.onDraggingOver();
}
},onOutEvent:function(){
_1.dnd.Source.superclass.onOutEvent.call(this);
_3.manager().outSource(this);
if(this.isDragging&&this.targetState!="Disabled"){
this.onDraggingOut();
}
},_markTargetAnchor:function(_2f){
if(this.current==this.targetAnchor&&this.before==_2f){
return;
}
if(this.targetAnchor){
this._removeItemClass(this.targetAnchor,this.before?"Before":"After");
}
this.targetAnchor=this.current;
this.targetBox=null;
this.before=_2f;
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
},_markDndStatus:function(_30){
this._changeState("Source",_30?"Copied":"Moved");
},_legalMouseDown:function(e){
if(!_1.mouseButtons.isLeft(e)){
return false;
}
if(!this.withHandles){
return true;
}
for(var _31=e.target;_31&&_31!==this.node;_31=_31.parentNode){
if(_1.hasClass(_31,"dojoDndHandle")){
return true;
}
if(_1.hasClass(_31,"dojoDndItem")||_1.hasClass(_31,"dojoDndIgnore")){
break;
}
}
return false;
}});
});
