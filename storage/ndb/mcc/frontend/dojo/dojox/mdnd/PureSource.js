//>>built
define("dojox/mdnd/PureSource",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/connect","dojo/_base/array","dojo/dom-class","dojo/dnd/common","dojo/dnd/Selector","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mdnd.PureSource",_8,{horizontal:false,copyOnly:true,skipForm:false,withHandles:false,isSource:true,targetState:"Disabled",generateText:true,constructor:function(_a,_b){
_3.mixin(this,_3.mixin({},_b));
var _c=this.accept;
this.isDragging=false;
this.mouseDown=false;
this.sourceState="";
_6.add(this.node,"dojoDndSource");
if(this.horizontal){
_6.add(this.node,"dojoDndHorizontal");
}
this.topics=[_4.subscribe("/dnd/cancel",this,"onDndCancel"),_4.subscribe("/dnd/drop",this,"onDndCancel")];
},onDndCancel:function(){
this.isDragging=false;
this.mouseDown=false;
delete this.mouseButton;
},copyState:function(_d){
return this.copyOnly||_d;
},destroy:function(){
dojox.mdnd.PureSource.superclass.destroy.call(this);
_5.forEach(this.topics,_4.unsubscribe);
this.targetAnchor=null;
},markupFactory:function(_e,_f){
_e._skipStartup=true;
return new dojox.mdnd.PureSource(_f,_e);
},onMouseMove:function(e){
if(this.isDragging){
return;
}
dojox.mdnd.PureSource.superclass.onMouseMove.call(this,e);
var m=_9.manager();
if(this.mouseDown&&!this.isDragging&&this.isSource){
var _10=this.getSelectedNodes();
if(_10.length){
m.startDrag(this,_10,this.copyState(_4.isCopyKey(e)));
this.isDragging=true;
}
}
},onMouseDown:function(e){
if(this._legalMouseDown(e)&&(!this.skipForm||!_7.isFormElement(e))){
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
_9.manager().overSource(this);
},onOutEvent:function(){
dojox.mdnd.PureSource.superclass.onOutEvent.call(this);
_9.manager().outSource(this);
},_markDndStatus:function(_11){
this._changeState("Source",_11?"Copied":"Moved");
},_legalMouseDown:function(e){
if(!this.withHandles){
return true;
}
for(var _12=e.target;_12&&!_6.contains(_12,"dojoDndItem");_12=_12.parentNode){
if(_6.contains(_12,"dojoDndHandle")){
return true;
}
}
return false;
}});
});
