//>>built
define("dojox/drawing/tools/Arrow",["dojo/_base/lang","../util/oo","../manager/_registry","./Line","../annotations/Arrow","../util/positioning"],function(_1,oo,_2,_3,_4,_5){
var _6=oo.declare(_3,function(_7){
if(this.arrowStart){
this.begArrow=new _4({stencil:this,idx1:0,idx2:1});
}
if(this.arrowEnd){
this.endArrow=new _4({stencil:this,idx1:1,idx2:0});
}
if(this.points.length){
this.render();
_7.label&&this.setLabel(_7.label);
}
},{draws:true,type:"dojox.drawing.tools.Arrow",baseRender:false,arrowStart:false,arrowEnd:true,labelPosition:function(){
var d=this.data;
var pt=_5.label({x:d.x1,y:d.y1},{x:d.x2,y:d.y2});
return {x:pt.x,y:pt.y};
},onUp:function(_8){
if(this.created||!this.shape){
return;
}
var p=this.points;
var _9=this.util.distance(p[0].x,p[0].y,p[1].x,p[1].y);
if(_9<this.minimumSize){
this.remove(this.shape,this.hit);
return;
}
var pt=this.util.snapAngle(_8,this.angleSnap/180);
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
this.renderedOnce=true;
this.onRender(this);
}});
_1.setObject("dojox.drawing.tools.Arrow",_6);
_6.setup={name:"dojox.drawing.tools.Arrow",tooltip:"Arrow Tool",iconClass:"iconArrow"};
_2.register(_6.setup,"tool");
return _6;
});
