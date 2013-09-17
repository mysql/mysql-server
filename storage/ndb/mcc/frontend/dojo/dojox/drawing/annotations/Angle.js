//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.annotations.Angle");
_3.drawing.annotations.Angle=_3.drawing.util.oo.declare(function(_4){
this.stencil=_4.stencil;
this.util=_4.stencil.util;
this.mouse=_4.stencil.mouse;
this.stencil.connectMult([["onDrag",this,"showAngle"],["onUp",this,"hideAngle"],["onTransformBegin",this,"showAngle"],["onTransform",this,"showAngle"],["onTransformEnd",this,"hideAngle"]]);
},{type:"dojox.drawing.tools.custom",angle:0,showAngle:function(){
if(!this.stencil.selected&&this.stencil.created){
return;
}
if(this.stencil.getRadius()<this.stencil.minimumSize){
this.hideAngle();
return;
}
var _5=this.getAngleNode();
var d=this.stencil.pointsToData();
var pt=_3.drawing.util.positioning.angle({x:d.x1,y:d.y1},{x:d.x2,y:d.y2});
var sc=this.mouse.scrollOffset();
var mx=this.stencil.getTransform();
var dx=mx.dx/this.mouse.zoom;
var dy=mx.dy/this.mouse.zoom;
pt.x/=this.mouse.zoom;
pt.y/=this.mouse.zoom;
var x=this.stencil._offX+pt.x-sc.left+dx;
var y=this.stencil._offY+pt.y-sc.top+dy;
_2.style(_5,{left:x+"px",top:y+"px",align:pt.align});
var _6=this.stencil.getAngle();
if(this.stencil.style.zAxis&&this.stencil.shortType=="vector"){
_5.innerHTML=this.stencil.data.cosphi>0?"out of":"into";
}else{
if(this.stencil.shortType=="line"){
_5.innerHTML=this.stencil.style.zAxis?"out of":Math.ceil(_6%180);
}else{
_5.innerHTML=Math.ceil(_6);
}
}
},getAngleNode:function(){
if(!this._angleNode){
this._angleNode=_2.create("span",null,_2.body());
_2.addClass(this._angleNode,"textAnnotation");
_2.style(this._angleNode,"opacity",1);
}
return this._angleNode;
},hideAngle:function(){
if(this._angleNode&&_2.style(this._angleNode,"opacity")>0.9){
_2.fadeOut({node:this._angleNode,duration:500,onEnd:function(_7){
_2.destroy(_7);
}}).play();
this._angleNode=null;
}
}});
});
