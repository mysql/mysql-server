//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.Ellipse");
_3.drawing.tools.Ellipse=_3.drawing.util.oo.declare(_3.drawing.stencil.Ellipse,function(){
},{draws:true,onDrag:function(_4){
var s=_4.start,e=_4;
var x=s.x<e.x?s.x:e.x,y=s.y<e.y?s.y:e.y,w=s.x<e.x?e.x-s.x:s.x-e.x,h=s.y<e.y?e.y-s.y:s.y-e.y;
if(this.keys.shift){
w=h=Math.max(w,h);
}
if(!this.keys.alt){
x+=w/2;
y+=h/2;
w/=2;
h/=2;
}else{
if(y-h<0){
h=y;
}
if(x-w<0){
w=x;
}
}
this.points=[{x:x-w,y:y-h},{x:x+w,y:y-h},{x:x+w,y:y+h},{x:x-w,y:y+h}];
this.render();
},onUp:function(_5){
if(this.created||!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
if(!this.shape){
var s=_5.start,e=this.minimumSize*2;
this.data={cx:s.x+e,cy:s.y+e,rx:e,ry:e};
this.dataToPoints();
this.render();
}else{
var o=this.pointsToData();
if(o.rx*2<this.minimumSize&&o.ry*2<this.minimumSize){
this.remove(this.shape,this.hit);
return;
}
}
this.onRender(this);
}});
_3.drawing.tools.Ellipse.setup={name:"dojox.drawing.tools.Ellipse",tooltip:"Ellipse Tool",iconClass:"iconEllipse"};
_3.drawing.register(_3.drawing.tools.Ellipse.setup,"tool");
});
