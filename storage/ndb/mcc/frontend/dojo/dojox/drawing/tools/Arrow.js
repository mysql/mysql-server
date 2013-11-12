//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.Arrow");
_3.drawing.tools.Arrow=_3.drawing.util.oo.declare(_3.drawing.tools.Line,function(_4){
if(this.arrowStart){
this.begArrow=new _3.drawing.annotations.Arrow({stencil:this,idx1:0,idx2:1});
}
if(this.arrowEnd){
this.endArrow=new _3.drawing.annotations.Arrow({stencil:this,idx1:1,idx2:0});
}
if(this.points.length){
this.render();
_4.label&&this.setLabel(_4.label);
}
},{draws:true,type:"dojox.drawing.tools.Arrow",baseRender:false,arrowStart:false,arrowEnd:true,labelPosition:function(){
var d=this.data;
var pt=_3.drawing.util.positioning.label({x:d.x1,y:d.y1},{x:d.x2,y:d.y2});
return {x:pt.x,y:pt.y};
},onUp:function(_5){
if(this.created||!this.shape){
return;
}
var p=this.points;
var _6=this.util.distance(p[0].x,p[0].y,p[1].x,p[1].y);
if(_6<this.minimumSize){
this.remove(this.shape,this.hit);
return;
}
var pt=this.util.snapAngle(_5,this.angleSnap/180);
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
this.renderedOnce=true;
this.onRender(this);
}});
_3.drawing.tools.Arrow.setup={name:"dojox.drawing.tools.Arrow",tooltip:"Arrow Tool",iconClass:"iconArrow"};
_3.drawing.register(_3.drawing.tools.Arrow.setup,"tool");
});
