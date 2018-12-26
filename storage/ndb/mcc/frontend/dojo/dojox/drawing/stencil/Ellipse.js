//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Ellipse");
_3.drawing.stencil.Ellipse=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
},{type:"dojox.drawing.stencil.Ellipse",anchorType:"group",baseRender:true,dataToPoints:function(o){
o=o||this.data;
var x=o.cx-o.rx,y=o.cy-o.ry,w=o.rx*2,h=o.ry*2;
this.points=[{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+h},{x:x,y:y+h}];
return this.points;
},pointsToData:function(p){
p=p||this.points;
var s=p[0];
var e=p[2];
this.data={cx:s.x+(e.x-s.x)/2,cy:s.y+(e.y-s.y)/2,rx:(e.x-s.x)*0.5,ry:(e.y-s.y)*0.5};
return this.data;
},_create:function(_5,d,_6){
this.remove(this[_5]);
this[_5]=this.container.createEllipse(d).setStroke(_6).setFill(_6.fill);
this._setNodeAtts(this[_5]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.data,this.style.currentHit);
this._create("shape",this.data,this.style.current);
}});
_3.drawing.register({name:"dojox.drawing.stencil.Ellipse"},"stencil");
});
