//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/tools/Arrow,dojox/drawing/util/positioning"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.custom.Vector");
_2.require("dojox.drawing.tools.Arrow");
_2.require("dojox.drawing.util.positioning");
_3.drawing.tools.custom.Vector=_3.drawing.util.oo.declare(_3.drawing.tools.Arrow,function(_4){
this.minimumSize=this.style.arrows.length;
this.addShadow({size:3,mult:2});
},{draws:true,type:"dojox.drawing.tools.custom.Vector",minimumSize:30,showAngle:true,changeAxis:function(_5){
_5=_5!==undefined?_5:this.style.zAxis?0:1;
if(_5==0){
this.style.zAxis=false;
this.data.cosphi=0;
}else{
this.style.zAxis=true;
var p=this.points;
var pt=this.zPoint();
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
}
this.render();
},_createZeroVector:function(_6,d,_7){
var s=_6=="hit"?this.minimumSize:this.minimumSize/6;
var f=_6=="hit"?_7.fill:null;
d={cx:this.data.x1,cy:this.data.y1,rx:s,ry:s};
this.remove(this[_6]);
this[_6]=this.container.createEllipse(d).setStroke(_7).setFill(f);
this.util.attr(this[_6],"drawingType","stencil");
},_create:function(_8,d,_9){
this.remove(this[_8]);
this[_8]=this.container.createLine(d).setStroke(_9);
this._setNodeAtts(this[_8]);
},onDrag:function(_a){
if(this.created){
return;
}
var x1=_a.start.x,y1=_a.start.y,x2=_a.x,y2=_a.y;
if(this.keys.shift&&!this.style.zAxis){
var pt=this.util.snapAngle(_a,45/180);
x2=pt.x;
y2=pt.y;
}
if(this.keys.alt){
var dx=x2>x1?((x2-x1)/2):((x1-x2)/-2);
var dy=y2>y1?((y2-y1)/2):((y1-y2)/-2);
x1-=dx;
x2-=dx;
y1-=dy;
y2-=dy;
}
if(this.style.zAxis){
var _b=this.zPoint(_a);
x2=_b.x;
y2=_b.y;
}
this.setPoints([{x:x1,y:y1},{x:x2,y:y2}]);
this.render();
},onTransform:function(_c){
if(!this._isBeingModified){
this.onTransformBegin();
}
this.setPoints(this.points);
this.render();
},anchorConstrain:function(x,y){
if(!this.style.zAxis){
return null;
}
var _d=this.style.zAngle*Math.PI/180;
var _e=x<0?x>-y:x<-y;
var dx=_e?x:-y/Math.tan(_d);
var dy=!_e?y:-Math.tan(_d)*x;
return {x:dx,y:dy};
},zPoint:function(_f){
if(_f===undefined){
if(!this.points[0]){
return null;
}
var d=this.pointsToData();
_f={start:{x:d.x1,y:d.y1},x:d.x2,y:d.y2};
}
var _10=this.util.length(_f);
var _11=this.util.angle(_f);
_11<0?_11=360+_11:_11;
_11=_11>135&&_11<315?this.style.zAngle:this.util.oppAngle(this.style.zAngle);
return this.util.pointOnCircle(_f.start.x,_f.start.y,_10,_11);
},pointsToData:function(p){
p=p||this.points;
var _12=0;
var obj={start:{x:p[0].x,y:p[0].y},x:p[1].x,y:p[1].y};
if(this.style.zAxis&&(this.util.length(obj)>this.minimumSize)){
var _13=this.util.angle(obj);
_13<0?_13=360+_13:_13;
_12=_13>135&&_13<315?1:-1;
}
this.data={x1:p[0].x,y1:p[0].y,x2:p[1].x,y2:p[1].y,cosphi:_12};
return this.data;
},dataToPoints:function(o){
o=o||this.data;
if(o.radius||o.angle){
var _14=0;
var pt=this.util.pointOnCircle(o.x,o.y,o.radius,o.angle);
if(this.style.zAxis||(o.cosphi&&o.cosphi!=0)){
this.style.zAxis=true;
_14=o.angle>135&&o.angle<315?1:-1;
}
this.data=o={x1:o.x,y1:o.y,x2:pt.x,y2:pt.y,cosphi:_14};
}
this.points=[{x:o.x1,y:o.y1},{x:o.x2,y:o.y2}];
return this.points;
},render:function(){
this.onBeforeRender(this);
if(this.getRadius()>=this.minimumSize){
this._create("hit",this.data,this.style.currentHit);
this._create("shape",this.data,this.style.current);
}else{
this.data.cosphi=0;
this._createZeroVector("hit",this.data,this.style.currentHit);
this._createZeroVector("shape",this.data,this.style.current);
}
},onUp:function(obj){
if(this.created||!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
if(!this.shape){
var d=100;
obj.start.x=this.style.zAxis?obj.start.x+d:obj.start.x;
obj.y=obj.y+d;
this.setPoints([{x:obj.start.x,y:obj.start.y},{x:obj.x,y:obj.y}]);
this.render();
}
if(this.getRadius()<this.minimumSize){
var p=this.points;
this.setPoints([{x:p[0].x,y:p[0].y},{x:p[0].x,y:p[0].y}]);
}else{
var p=this.points;
var pt=this.style.zAxis?this.zPoint(obj):this.util.snapAngle(obj,this.angleSnap/180);
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
}
this.renderedOnce=true;
this.onRender(this);
}});
_3.drawing.tools.custom.Vector.setup={name:"dojox.drawing.tools.custom.Vector",tooltip:"Vector Tool",iconClass:"iconVector"};
if(_3.drawing.defaults.zAxisEnabled){
_3.drawing.tools.custom.Vector.setup.secondary={name:"vectorSecondary",label:"z-axis",funct:function(_15){
_15.selected?this.zDeselect(_15):this.zSelect(_15);
var _16=this.drawing.stencils.selectedStencils;
for(var nm in _16){
if(_16[nm].shortType=="vector"&&(_16[nm].style.zAxis!=_3.drawing.defaults.zAxis)){
var s=_16[nm];
s.changeAxis();
if(s.style.zAxis){
s.deselect();
s.select();
}
}
}
},setup:function(){
var _17=_3.drawing.defaults.zAxis;
this.zSelect=function(_18){
if(!_18.enabled){
return;
}
_17=true;
_3.drawing.defaults.zAxis=true;
_18.select();
this.vectorTest();
this.zSelected=_18;
};
this.zDeselect=function(_19){
if(!_19.enabled){
return;
}
_17=false;
_3.drawing.defaults.zAxis=false;
_19.deselect();
this.vectorTest();
this.zSelected=null;
};
this.vectorTest=function(){
_2.forEach(this.buttons,function(b){
if(b.toolType=="vector"&&b.selected){
this.drawing.currentStencil.style.zAxis=_17;
}
},this);
};
_2.connect(this,"onRenderStencil",this,function(){
if(this.zSelected){
this.zDeselect(this.zSelected);
}
});
var c=_2.connect(this.drawing,"onSurfaceReady",this,function(){
_2.disconnect(c);
_2.connect(this.drawing.stencils,"onSelect",this,function(_1a){
if(_1a.shortType=="vector"){
if(_1a.style.zAxis){
_2.forEach(this.buttons,function(b){
if(b.toolType=="vectorSecondary"){
this.zSelect(b);
}
},this);
}else{
_2.forEach(this.buttons,function(b){
if(b.toolType=="vectorSecondary"){
this.zDeselect(b);
}
},this);
}
}
});
});
},postSetup:function(btn){
_2.connect(btn,"enable",function(){
_3.drawing.defaults.zAxisEnabled=true;
});
_2.connect(btn,"disable",function(){
_3.drawing.defaults.zAxisEnabled=false;
});
}};
}
_3.drawing.register(_3.drawing.tools.custom.Vector.setup,"tool");
});
