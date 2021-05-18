//>>built
define("dojox/drawing/annotations/Angle",["dojo","../util/oo","../util/positioning"],function(_1,oo,_2){
return oo.declare(function(_3){
this.stencil=_3.stencil;
this.util=_3.stencil.util;
this.mouse=_3.stencil.mouse;
this.stencil.connectMult([["onDrag",this,"showAngle"],["onUp",this,"hideAngle"],["onTransformBegin",this,"showAngle"],["onTransform",this,"showAngle"],["onTransformEnd",this,"hideAngle"]]);
},{type:"dojox.drawing.tools.custom",angle:0,showAngle:function(){
if(!this.stencil.selected&&this.stencil.created){
return;
}
if(this.stencil.getRadius()<this.stencil.minimumSize){
this.hideAngle();
return;
}
var _4=this.getAngleNode();
var d=this.stencil.pointsToData();
var pt=_2.angle({x:d.x1,y:d.y1},{x:d.x2,y:d.y2});
var sc=this.mouse.scrollOffset();
var mx=this.stencil.getTransform();
var dx=mx.dx/this.mouse.zoom;
var dy=mx.dy/this.mouse.zoom;
pt.x/=this.mouse.zoom;
pt.y/=this.mouse.zoom;
var x=this.stencil._offX+pt.x-sc.left+dx;
var y=this.stencil._offY+pt.y-sc.top+dy;
_1.style(_4,{left:x+"px",top:y+"px",align:pt.align});
var _5=this.stencil.getAngle();
if(this.stencil.style.zAxis&&this.stencil.shortType=="vector"){
_4.innerHTML=this.stencil.data.cosphi>0?"out of":"into";
}else{
if(this.stencil.shortType=="line"){
_4.innerHTML=this.stencil.style.zAxis?"out of":Math.ceil(_5%180);
}else{
_4.innerHTML=Math.ceil(_5);
}
}
},getAngleNode:function(){
if(!this._angleNode){
this._angleNode=_1.create("span",null,_1.body());
_1.addClass(this._angleNode,"textAnnotation");
_1.style(this._angleNode,"opacity",1);
}
return this._angleNode;
},hideAngle:function(){
if(this._angleNode&&_1.style(this._angleNode,"opacity")>0.9){
_1.fadeOut({node:this._angleNode,duration:500,onEnd:function(_6){
_1.destroy(_6);
}}).play();
this._angleNode=null;
}
}});
});
