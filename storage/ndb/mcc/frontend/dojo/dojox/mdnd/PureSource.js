//>>built
define("dojox/mdnd/PureSource",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/connect","dojo/_base/array","dojo/dom-class","dojo/dnd/common","dojo/dnd/Selector","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_2("dojox.mdnd.PureSource",_8,{horizontal:false,copyOnly:true,skipForm:false,withHandles:false,isSource:true,targetState:"Disabled",generateText:true,constructor:function(_b,_c){
_3.mixin(this,_3.mixin({},_c));
var _d=this.accept;
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
},copyState:function(_e){
return this.copyOnly||_e;
},destroy:function(){
_a.superclass.destroy.call(this);
_5.forEach(this.topics,_4.unsubscribe);
this.targetAnchor=null;
},markupFactory:function(_f,_10){
_f._skipStartup=true;
return new _a(_10,_f);
},onMouseMove:function(e){
if(this.isDragging){
return;
}
_a.superclass.onMouseMove.call(this,e);
var m=_9.manager();
if(this.mouseDown&&!this.isDragging&&this.isSource){
var _11=this.getSelectedNodes();
if(_11.length){
m.startDrag(this,_11,this.copyState(_4.isCopyKey(e)));
this.isDragging=true;
}
}
},onMouseDown:function(e){
if(this._legalMouseDown(e)&&(!this.skipForm||!_7.isFormElement(e))){
this.mouseDown=true;
this.mouseButton=e.button;
_a.superclass.onMouseDown.call(this,e);
}
},onMouseUp:function(e){
if(this.mouseDown){
this.mouseDown=false;
_a.superclass.onMouseUp.call(this,e);
}
},onOverEvent:function(){
_a.superclass.onOverEvent.call(this);
_9.manager().overSource(this);
},onOutEvent:function(){
_a.superclass.onOutEvent.call(this);
_9.manager().outSource(this);
},_markDndStatus:function(_12){
this._changeState("Source",_12?"Copied":"Moved");
},_legalMouseDown:function(e){
if(!this.withHandles){
return true;
}
for(var _13=e.target;_13&&!_6.contains(_13,"dojoDndItem");_13=_13.parentNode){
if(_6.contains(_13,"dojoDndHandle")){
return true;
}
}
return false;
}});
return _a;
});
