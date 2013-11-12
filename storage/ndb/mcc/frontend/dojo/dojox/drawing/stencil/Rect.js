//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Rect");
_3.drawing.stencil.Rect=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
if(this.points.length){
}
},{type:"dojox.drawing.stencil.Rect",anchorType:"group",baseRender:true,dataToPoints:function(d){
d=d||this.data;
this.points=[{x:d.x,y:d.y},{x:d.x+d.width,y:d.y},{x:d.x+d.width,y:d.y+d.height},{x:d.x,y:d.y+d.height}];
return this.points;
},pointsToData:function(p){
p=p||this.points;
var s=p[0];
var e=p[2];
this.data={x:s.x,y:s.y,width:e.x-s.x,height:e.y-s.y,r:this.data.r||0};
return this.data;
},_create:function(_5,d,_6){
this.remove(this[_5]);
this[_5]=this.container.createRect(d).setStroke(_6).setFill(_6.fill);
this._setNodeAtts(this[_5]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.data,this.style.currentHit);
this._create("shape",this.data,this.style.current);
}});
_3.drawing.register({name:"dojox.drawing.stencil.Rect"},"stencil");
});
