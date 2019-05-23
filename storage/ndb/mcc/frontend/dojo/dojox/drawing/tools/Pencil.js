//>>built
define("dojox/drawing/tools/Pencil",["dojo/_base/lang","../util/oo","../manager/_registry","../stencil/Path"],function(_1,oo,_2,_3){
var _4=oo.declare(_3,function(){
this._started=false;
},{draws:true,minDist:15,onDown:function(_5){
this._started=true;
var p={x:_5.x,y:_5.y};
this.points=[p];
this.lastPoint=p;
this.revertRenderHit=this.renderHit;
this.renderHit=false;
this.closePath=false;
},onDrag:function(_6){
if(!this._started||this.minDist>this.util.distance(_6.x,_6.y,this.lastPoint.x,this.lastPoint.y)){
return;
}
var p={x:_6.x,y:_6.y};
this.points.push(p);
this.render();
this.checkClosePoint(this.points[0],_6);
this.lastPoint=p;
},onUp:function(_7){
if(!this._started){
return;
}
if(!this.points||this.points.length<2){
this._started=false;
this.points=[];
return;
}
var _8=this.getBounds();
if(_8.w<this.minimumSize&&_8.h<this.minimumSize){
this.remove(this.hit,this.shape,this.closeGuide);
this._started=false;
this.setPoints([]);
return;
}
if(this.checkClosePoint(this.points[0],_7,true)){
this.closePath=true;
}
this.renderHit=this.revertRenderHit;
this.renderedOnce=true;
this.render();
this.onRender(this);
}});
_1.setObject("dojox.drawing.tools.Pencil",_4);
_4.setup={name:"dojox.drawing.tools.Pencil",tooltip:"Pencil Tool",iconClass:"iconLine"};
_2.register(_4.setup,"tool");
return _4;
});
