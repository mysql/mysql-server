//>>built
define("dojox/drawing/tools/custom/Vector",["dojo","../../util/oo","../../manager/_registry","../../util/positioning","../Arrow"],function(_1,oo,_2,_3,_4){
var _5=oo.declare(_4,function(_6){
this.minimumSize=this.style.arrows.length;
this.addShadow({size:3,mult:2});
},{draws:true,type:"dojox.drawing.tools.custom.Vector",minimumSize:30,showAngle:true,changeAxis:function(_7){
_7=_7!==undefined?_7:this.style.zAxis?0:1;
if(_7==0){
this.style.zAxis=false;
this.data.cosphi=0;
}else{
this.style.zAxis=true;
var p=this.points;
var pt=this.zPoint();
this.setPoints([{x:p[0].x,y:p[0].y},{x:pt.x,y:pt.y}]);
}
this.render();
},_createZeroVector:function(_8,d,_9){
var s=_8=="hit"?this.minimumSize:this.minimumSize/6;
var f=_8=="hit"?_9.fill:null;
d={cx:this.data.x1,cy:this.data.y1,rx:s,ry:s};
this.remove(this[_8]);
this[_8]=this.container.createEllipse(d).setStroke(_9).setFill(f);
this.util.attr(this[_8],"drawingType","stencil");
},_create:function(_a,d,_b){
this.remove(this[_a]);
this[_a]=this.container.createLine(d).setStroke(_b);
this._setNodeAtts(this[_a]);
},onDrag:function(_c){
if(this.created){
return;
}
var x1=_c.start.x,y1=_c.start.y,x2=_c.x,y2=_c.y;
if(this.keys.shift&&!this.style.zAxis){
var pt=this.util.snapAngle(_c,45/180);
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
var _d=this.zPoint(_c);
x2=_d.x;
y2=_d.y;
}
this.setPoints([{x:x1,y:y1},{x:x2,y:y2}]);
this.render();
},onTransform:function(_e){
if(!this._isBeingModified){
this.onTransformBegin();
}
this.setPoints(this.points);
this.render();
},anchorConstrain:function(x,y){
if(!this.style.zAxis){
return null;
}
var _f=this.style.zAngle*Math.PI/180;
var _10=x<0?x>-y:x<-y;
var dx=_10?x:-y/Math.tan(_f);
var dy=!_10?y:-Math.tan(_f)*x;
return {x:dx,y:dy};
},zPoint:function(obj){
if(obj===undefined){
if(!this.points[0]){
return null;
}
var d=this.pointsToData();
obj={start:{x:d.x1,y:d.y1},x:d.x2,y:d.y2};
}
var _11=this.util.length(obj);
var _12=_3.angle(obj);
_12<0?_12=360+_12:_12;
_12=_12>135&&_12<315?this.style.zAngle:this.util.oppAngle(this.style.zAngle);
return this.util.pointOnCircle(obj.start.x,obj.start.y,_11,_12);
},pointsToData:function(p){
p=p||this.points;
var _13=0;
var obj={start:{x:p[0].x,y:p[0].y},x:p[1].x,y:p[1].y};
if(this.style.zAxis&&(this.util.length(obj)>this.minimumSize)){
var _14=_3.angle(obj);
_14<0?_14=360+_14:_14;
_13=_14>135&&_14<315?1:-1;
}
this.data={x1:p[0].x,y1:p[0].y,x2:p[1].x,y2:p[1].y,cosphi:_13};
return this.data;
},dataToPoints:function(o){
o=o||this.data;
if(o.radius||o.angle){
var _15=0;
var pt=this.util.pointOnCircle(o.x,o.y,o.radius,o.angle);
if(this.style.zAxis||(o.cosphi&&o.cosphi!=0)){
this.style.zAxis=true;
_15=o.angle>135&&o.angle<315?1:-1;
}
this.data=o={x1:o.x,y1:o.y,x2:pt.x,y2:pt.y,cosphi:_15};
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
_1.setObject("dojox.drawing.tools.custom.Vector",_5);
_5.setup={name:"dojox.drawing.tools.custom.Vector",tooltip:"Vector Tool",iconClass:"iconVector"};
if(0&&dojox.drawing.defaults.zAxisEnabled){
dojox.drawing.tools.custom.Vector.setup.secondary={name:"vectorSecondary",label:"z-axis",funct:function(_16){
_16.selected?this.zDeselect(_16):this.zSelect(_16);
var _17=this.drawing.stencils.selectedStencils;
for(var nm in _17){
if(_17[nm].shortType=="vector"&&(_17[nm].style.zAxis!=dojox.drawing.defaults.zAxis)){
var s=_17[nm];
s.changeAxis();
if(s.style.zAxis){
s.deselect();
s.select();
}
}
}
},setup:function(){
var _18=dojox.drawing.defaults.zAxis;
this.zSelect=function(_19){
if(!_19.enabled){
return;
}
_18=true;
dojox.drawing.defaults.zAxis=true;
_19.select();
this.vectorTest();
this.zSelected=_19;
};
this.zDeselect=function(_1a){
if(!_1a.enabled){
return;
}
_18=false;
dojox.drawing.defaults.zAxis=false;
_1a.deselect();
this.vectorTest();
this.zSelected=null;
};
this.vectorTest=function(){
_1.forEach(this.buttons,function(b){
if(b.toolType=="vector"&&b.selected){
this.drawing.currentStencil.style.zAxis=_18;
}
},this);
};
_1.connect(this,"onRenderStencil",this,function(){
if(this.zSelected){
this.zDeselect(this.zSelected);
}
});
var c=_1.connect(this.drawing,"onSurfaceReady",this,function(){
_1.disconnect(c);
_1.connect(this.drawing.stencils,"onSelect",this,function(_1b){
if(_1b.shortType=="vector"){
if(_1b.style.zAxis){
_1.forEach(this.buttons,function(b){
if(b.toolType=="vectorSecondary"){
this.zSelect(b);
}
},this);
}else{
_1.forEach(this.buttons,function(b){
if(b.toolType=="vectorSecondary"){
this.zDeselect(b);
}
},this);
}
}
});
});
},postSetup:function(btn){
_1.connect(btn,"enable",function(){
dojox.drawing.defaults.zAxisEnabled=true;
});
_1.connect(btn,"disable",function(){
dojox.drawing.defaults.zAxisEnabled=false;
});
}};
}
_2.register(_5.setup,"tool");
return _5;
});
