//>>built
define("dojox/mobile/_ItemBase",["dojo/_base/kernel","dojo/_base/config","dojo/_base/declare","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./TransitionEvent","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _3("dojox.mobile._ItemBase",[_7,_6,_5],{icon:"",iconPos:"",alt:"",href:"",hrefTarget:"",moveTo:"",scene:"",clickable:false,url:"",urlTarget:"",transition:"",transitionDir:1,transitionOptions:null,callback:null,sync:true,label:"",toggle:false,_duration:800,inheritParams:function(){
var _a=this.getParent();
if(_a){
if(!this.transition){
this.transition=_a.transition;
}
if(this.icon&&_a.iconBase&&_a.iconBase.charAt(_a.iconBase.length-1)==="/"){
this.icon=_a.iconBase+this.icon;
}
if(!this.icon){
this.icon=_a.iconBase;
}
if(!this.iconPos){
this.iconPos=_a.iconPos;
}
}
},select:function(){
},deselect:function(){
},defaultClickAction:function(e){
if(this.toggle){
if(this.selected){
this.deselect();
}else{
this.select();
}
}else{
if(!this.selected){
this.select();
if(!this.selectOne){
var _b=this;
setTimeout(function(){
_b.deselect();
},this._duration);
}
var _c;
if(this.moveTo||this.href||this.url||this.scene){
_c={moveTo:this.moveTo,href:this.href,url:this.url,scene:this.scene,transition:this.transition,transitionDir:this.transitionDir};
}else{
if(this.transitionOptions){
_c=this.transitionOptions;
}
}
if(_c){
return new _8(this.domNode,_c,e).dispatch();
}
}
}
},getParent:function(){
var _d=this.srcNodeRef||this.domNode;
return _d&&_d.parentNode?_4.getEnclosingWidget(_d.parentNode):null;
},setTransitionPos:function(e){
var w=this;
while(true){
w=w.getParent();
if(!w||w instanceof _9){
break;
}
}
if(w){
w.clickedPosX=e.clientX;
w.clickedPosY=e.clientY;
}
},transitionTo:function(_e,_f,url,_10){
if(_2.isDebug){
var _11=arguments.callee._ach||(arguments.callee._ach={}),_12=(arguments.callee.caller||"unknown caller").toString();
if(!_11[_12]){
_1.deprecated(this.declaredClass+"::transitionTo() is deprecated."+_12,"","2.0");
_11[_12]=true;
}
}
new _8(this.domNode,{moveTo:_e,href:_f,url:url,scene:_10,transition:this.transition,transitionDir:this.transitionDir}).dispatch();
}});
});
