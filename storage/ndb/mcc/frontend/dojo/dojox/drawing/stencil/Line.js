//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Line");
_3.drawing.stencil.Line=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
},{type:"dojox.drawing.stencil.Line",anchorType:"single",baseRender:true,dataToPoints:function(o){
o=o||this.data;
if(o.radius||o.angle){
var pt=this.util.pointOnCircle(o.x,o.y,o.radius,o.angle);
this.data=o={x1:o.x,y1:o.y,x2:pt.x,y2:pt.y};
}
this.points=[{x:o.x1,y:o.y1},{x:o.x2,y:o.y2}];
return this.points;
},pointsToData:function(p){
p=p||this.points;
this.data={x1:p[0].x,y1:p[0].y,x2:p[1].x,y2:p[1].y};
return this.data;
},_create:function(_5,d,_6){
this.remove(this[_5]);
this[_5]=this.container.createLine(d).setStroke(_6);
this._setNodeAtts(this[_5]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.data,this.style.currentHit);
this._create("shape",this.data,this.style.current);
}});
_3.drawing.register({name:"dojox.drawing.stencil.Line"},"stencil");
});
