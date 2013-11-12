//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.drawing.Silverlight");
_3.drawing.plugins.drawing.Silverlight=_3.drawing.util.oo.declare(function(_4){
if(_3.gfx.renderer!="silverlight"){
return;
}
this.mouse=_4.mouse;
this.stencils=_4.stencils;
this.anchors=_4.anchors;
this.canvas=_4.canvas;
this.util=_4.util;
_2.connect(this.stencils,"register",this,function(_5){
var c1,c2,c3,c4,c5,_6=this;
var _7=function(){
c1=_5.container.connect("onmousedown",function(_8){
_8.superTarget=_5;
_6.mouse.down(_8);
});
};
_7();
c2=_2.connect(_5,"setTransform",this,function(){
});
c3=_2.connect(_5,"onBeforeRender",function(){
});
c4=_2.connect(_5,"onRender",this,function(){
});
c5=_2.connect(_5,"destroy",this,function(){
_2.forEach([c1,c2,c3,c4,c5],_2.disconnect,_2);
});
});
_2.connect(this.anchors,"onAddAnchor",this,function(_9){
var c1=_9.shape.connect("onmousedown",this.mouse,function(_a){
_a.superTarget=_9;
this.down(_a);
});
var c2=_2.connect(_9,"disconnectMouse",this,function(){
_2.disconnect(c1);
_2.disconnect(c2);
});
});
this.mouse._down=function(_b){
var _c=this._getXY(_b);
var x=_c.x-this.origin.x;
var y=_c.y-this.origin.y;
x*=this.zoom;
y*=this.zoom;
this.origin.startx=x;
this.origin.starty=y;
this._lastx=x;
this._lasty=y;
this.drawingType=this.util.attr(_b,"drawingType")||"";
var id=this._getId(_b);
var _d={x:x,y:y,id:id};
this.onDown(_d);
this._clickTime=new Date().getTime();
if(this._lastClickTime){
if(this._clickTime-this._lastClickTime<this.doublClickSpeed){
var _e=this.eventName("doubleClick");
console.warn("DOUBLE CLICK",_e,_d);
this._broadcastEvent(_e,_d);
}else{
}
}
this._lastClickTime=this._clickTime;
};
this.mouse.down=function(_f){
clearTimeout(this.__downInv);
if(this.util.attr(_f,"drawingType")=="surface"){
this.__downInv=setTimeout(_2.hitch(this,function(){
this._down(_f);
}),500);
return;
}
this._down(_f);
};
this.mouse._getXY=function(evt){
if(evt.pageX){
return {x:evt.pageX,y:evt.pageY,cancelBubble:true};
}
for(var nm in evt){
}
if(evt.x!==undefined){
return {x:evt.x+this.origin.x,y:evt.y+this.origin.y};
}else{
return {x:evt.pageX,y:evt.pageY};
}
};
this.mouse._getId=function(evt){
return this.util.attr(evt,"id");
};
this.util.attr=function(_10,_11,_12,_13){
if(!_10){
return false;
}
try{
var t;
if(_10.superTarget){
t=_10.superTarget;
}else{
if(_10.superClass){
t=_10.superClass;
}else{
if(_10.target){
t=_10.target;
}else{
t=_10;
}
}
}
if(_12!==undefined){
_10[_11]=_12;
return _12;
}
if(t.tagName){
if(_11=="drawingType"&&t.tagName.toLowerCase()=="object"){
return "surface";
}
var r=_2.attr(t,_11);
}
var r=t[_11];
return r;
}
catch(e){
if(!_13){
}
return false;
}
};
},{});
});
