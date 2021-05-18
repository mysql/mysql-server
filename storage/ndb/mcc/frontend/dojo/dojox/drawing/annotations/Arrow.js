//>>built
define("dojox/drawing/annotations/Arrow",["../util/oo","../stencil/Path"],function(oo,_1){
return oo.declare(_1,function(_2){
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
},{idx1:0,idx2:1,subShape:true,minimumSize:30,arrowHead:function(x1,y1,x2,y2,_3){
var _4={start:{x:x1,y:y1},x:x2,y:y2};
var _5=this.util.angle(_4);
var _6=this.util.length(_4);
var al=_3.arrows.length;
var aw=_3.arrows.width/2;
if(_6<al){
al=_6/2;
}
var p1=this.util.pointOnCircle(x2,y2,-al,_5-aw);
var p2=this.util.pointOnCircle(x2,y2,-al,_5+aw);
return [{x:x2,y:y2},p1,p2];
}});
});
