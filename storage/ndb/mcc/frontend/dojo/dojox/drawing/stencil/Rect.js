//>>built
define("dojox/drawing/stencil/Rect",["dojo/_base/lang","../util/oo","./_Base","../manager/_registry"],function(_1,oo,_2,_3){
var _4=oo.declare(_2,function(_5){
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
},_create:function(_6,d,_7){
this.remove(this[_6]);
this[_6]=this.container.createRect(d).setStroke(_7).setFill(_7.fill);
this._setNodeAtts(this[_6]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.data,this.style.currentHit);
this._create("shape",this.data,this.style.current);
}});
_1.setObject("dojox.drawing.stencil.Rect",_4);
_3.register({name:"dojox.drawing.stencil.Rect"},"stencil");
return _4;
});
