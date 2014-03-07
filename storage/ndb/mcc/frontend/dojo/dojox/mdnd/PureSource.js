//>>built
define("dojox/mdnd/PureSource",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","dojo/_base/connect","dojo/_base/array","dojo/dnd/Selector","dojo/dnd/Manager"],function(_1){
return _1.declare("dojox.mdnd.PureSource",_1.dnd.Selector,{horizontal:false,copyOnly:true,skipForm:false,withHandles:false,isSource:true,targetState:"Disabled",generateText:true,constructor:function(_2,_3){
_1.mixin(this,_1.mixin({},_3));
var _4=this.accept;
this.isDragging=false;
this.mouseDown=false;
this.sourceState="";
_1.addClass(this.node,"dojoDndSource");
if(this.horizontal){
_1.addClass(this.node,"dojoDndHorizontal");
}
this.topics=[_1.subscribe("/dnd/cancel",this,"onDndCancel"),_1.subscribe("/dnd/drop",this,"onDndCancel")];
},onDndCancel:function(){
this.isDragging=false;
this.mouseDown=false;
delete this.mouseButton;
},copyState:function(_5){
return this.copyOnly||_5;
},destroy:function(){
dojox.mdnd.PureSource.superclass.destroy.call(this);
_1.forEach(this.topics,_1.unsubscribe);
this.targetAnchor=null;
},markupFactory:function(_6,_7){
_6._skipStartup=true;
return new dojox.mdnd.PureSource(_7,_6);
},onMouseMove:function(e){
if(this.isDragging){
return;
}
dojox.mdnd.PureSource.superclass.onMouseMove.call(this,e);
var m=_1.dnd.manager();
if(this.mouseDown&&!this.isDragging&&this.isSource){
var _8=this.getSelectedNodes();
if(_8.length){
m.startDrag(this,_8,this.copyState(_1.isCopyKey(e)));
this.isDragging=true;
}
}
},onMouseDown:function(e){
if(this._legalMouseDown(e)&&(!this.skipForm||!_1.dnd.isFormElement(e))){
this.mouseDown=true;
this.mouseButton=e.button;
dojox.mdnd.PureSource.superclass.onMouseDown.call(this,e);
}
},onMouseUp:function(e){
if(this.mouseDown){
this.mouseDown=false;
dojox.mdnd.PureSource.superclass.onMouseUp.call(this,e);
}
},onOverEvent:function(){
dojox.mdnd.PureSource.superclass.onOverEvent.call(this);
_1.dnd.manager().overSource(this);
},onOutEvent:function(){
dojox.mdnd.PureSource.superclass.onOutEvent.call(this);
_1.dnd.manager().outSource(this);
},_markDndStatus:function(_9){
this._changeState("Source",_9?"Copied":"Moved");
},_legalMouseDown:function(e){
if(!this.withHandles){
return true;
}
for(var _a=e.target;_a&&!_1.hasClass(_a,"dojoDndItem");_a=_a.parentNode){
if(_1.hasClass(_a,"dojoDndHandle")){
return true;
}
}
return false;
}});
});
