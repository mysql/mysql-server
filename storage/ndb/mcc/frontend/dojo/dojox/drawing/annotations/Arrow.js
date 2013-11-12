//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/stencil/Path"],function(_1,_2,_3){
_2.provide("dojox.drawing.annotations.Arrow");
_2.require("dojox.drawing.stencil.Path");
_3.drawing.annotations.Arrow=_3.drawing.util.oo.declare(_3.drawing.stencil.Path,function(_4){
this.stencil.connectMult([[this.stencil,"select",this,"select"],[this.stencil,"deselect",this,"deselect"],[this.stencil,"render",this,"render"],[this.stencil,"onDelete",this,"destroy"]]);
this.connect("onBeforeRender",this,function(){
var o=this.stencil.points[this.idx1];
var c=this.stencil.points[this.idx2];
if(this.stencil.getRadius()>=this.minimumSize){
this.points=this.arrowHead(c.x,c.y,o.x,o.y,this.style);
}else{
this.points=[];
}
});
},{idx1:0,idx2:1,subShape:true,minimumSize:30,arrowHead:function(x1,y1,x2,y2,_5){
var _6={start:{x:x1,y:y1},x:x2,y:y2};
var _7=this.util.angle(_6);
var _8=this.util.length(_6);
var al=_5.arrows.length;
var aw=_5.arrows.width/2;
if(_8<al){
al=_8/2;
}
var p1=this.util.pointOnCircle(x2,y2,-al,_7-aw);
var p2=this.util.pointOnCircle(x2,y2,-al,_7+aw);
return [{x:x2,y:y2},p1,p2];
}});
});
