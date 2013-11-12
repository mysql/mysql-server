//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.Line");
_3.drawing.tools.Line=_3.drawing.util.oo.declare(_3.drawing.stencil.Line,function(){
},{draws:true,showAngle:true,onTransformEnd:function(_4){
this._toggleSelected();
if(this.getRadius()<this.minimumSize){
var p=this.points;
this.setPoints([{x:p[0].x,y:p[0].y},{x:p[0].x,y:p[0].y}]);
}else{
var d=this.data;
var _5={start:{x:d.x1,y:d.y1},x:d.x2,y:d.y2};
var pt=this.util.snapAngle(_5,this.angleSnap/180);
this.setPoints([{x:d.x1,y:d.y1},{x:pt.x,y:pt.y}]);
this._isBeingModified=false;
this.onModify(this);
}
},onDrag:function(_6){
if(this.created){
return;
}
var x1=_6.start.x,y1=_6.start.y,x2=_6.x,y2=_6.y;
if(this.keys.shift){
var pt=this.util.snapAngle(_6,45/180);
x2=pt.x;
y2=pt.y;
}
if(this.keys.alt){
var dx=x2>x1?((x2-x1)/2):((x1-x2)/-2);
var dy=y2>y1?((y2-y1)/2):((y1-y2)/-2);
x1-=dx;
x2-=dx;
y1-=dy;
y2-=dy;
}
this.setPoints([{x:x1,y:y1},{x:x2,y:y2}]);
this.render();
},onUp:function(_7){
if(this.created||!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
if(!this.shape){
var s=_7.start,e=this.minimumSize*4;
this.setPoints([{x:s.x,y:s.y+e},{x:s.x,y:s.y}]);
this.render();
}else{
if(this.getRadius()<this.minimumSize){
this.remove(this.shape,this.hit);
return;
}
}
var pt=this.util.snapAngle(_7,this.angleSnap/180);
var p=this.points;
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
this.renderedOnce=true;
this.onRender(this);
}});
_3.drawing.tools.Line.setup={name:"dojox.drawing.tools.Line",tooltip:"Line Tool",iconClass:"iconLine"};
_3.drawing.register(_3.drawing.tools.Line.setup,"tool");
});
