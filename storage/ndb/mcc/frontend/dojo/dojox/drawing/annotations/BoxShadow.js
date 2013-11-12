//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.annotations.BoxShadow");
_3.drawing.annotations.BoxShadow=_3.drawing.util.oo.declare(function(_4){
this.stencil=_4.stencil;
this.util=_4.stencil.util;
this.mouse=_4.stencil.mouse;
this.style=_4.stencil.style;
var _5={size:6,mult:4,alpha:0.05,place:"BR",color:"#646464"};
delete _4.stencil;
this.options=_2.mixin(_5,_4);
this.options.color=new _2.Color(this.options.color);
this.options.color.a=this.options.alpha;
switch(this.stencil.shortType){
case "image":
case "rect":
this.method="createForRect";
break;
case "ellipse":
this.method="createForEllipse";
break;
case "line":
this.method="createForLine";
break;
case "path":
this.method="createForPath";
break;
case "vector":
this.method="createForZArrow";
break;
default:
console.warn("A shadow cannot be made for Stencil type ",this.stencil.type);
}
if(this.method){
this.render();
this.stencil.connectMult([[this.stencil,"onTransform",this,"onTransform"],this.method=="createForZArrow"?[this.stencil,"render",this,"render"]:[this.stencil,"render",this,"onRender"],[this.stencil,"onDelete",this,"destroy"]]);
}
},{showing:true,render:function(){
if(this.container){
this.container.removeShape();
}
this.container=this.stencil.container.createGroup();
this.container.moveToBack();
var o=this.options,_6=o.size,_7=o.mult,d=this.method=="createForPath"?this.stencil.points:this.stencil.data,r=d.r||1,p=o.place,c=o.color;
this[this.method](o,_6,_7,d,r,p,c);
},hide:function(){
if(this.showing){
this.showing=false;
this.container.removeShape();
}
},show:function(){
if(!this.showing){
this.showing=true;
this.stencil.container.add(this.container);
}
},createForPath:function(o,_8,_9,_a,r,p,c){
var sh=_8*_9/4,_b=/B/.test(p)?sh:/T/.test(p)?sh*-1:0,_c=/R/.test(p)?sh:/L/.test(p)?sh*-1:0;
var _d=true;
for(var i=1;i<=_8;i++){
var _e=i*_9;
if(_3.gfx.renderer=="svg"){
var _f=[];
_2.forEach(_a,function(o,i){
if(i==0){
_f.push("M "+(o.x+_c)+" "+(o.y+_b));
}else{
var cmd=o.t||"L ";
_f.push(cmd+(o.x+_c)+" "+(o.y+_b));
}
},this);
if(_d){
_f.push("Z");
}
this.container.createPath(_f.join(", ")).setStroke({width:_e,color:c,cap:"round"});
}else{
var pth=this.container.createPath({}).setStroke({width:_e,color:c,cap:"round"});
_2.forEach(this.points,function(o,i){
if(i==0||o.t=="M"){
pth.moveTo(o.x+_c,o.y+_b);
}else{
if(o.t=="Z"){
_d&&pth.closePath();
}else{
pth.lineTo(o.x+_c,o.y+_b);
}
}
},this);
_d&&pth.closePath();
}
}
},createForLine:function(o,_10,_11,d,r,p,c){
var sh=_10*_11/4,shy=/B/.test(p)?sh:/T/.test(p)?sh*-1:0,shx=/R/.test(p)?sh:/L/.test(p)?sh*-1:0;
for(var i=1;i<=_10;i++){
var _12=i*_11;
this.container.createLine({x1:d.x1+shx,y1:d.y1+shy,x2:d.x2+shx,y2:d.y2+shy}).setStroke({width:_12,color:c,cap:"round"});
}
},createForEllipse:function(o,_13,_14,d,r,p,c){
var sh=_13*_14/8,shy=/B/.test(p)?sh:/T/.test(p)?sh*-1:0,shx=/R/.test(p)?sh*0.8:/L/.test(p)?sh*-0.8:0;
for(var i=1;i<=_13;i++){
var _15=i*_14;
this.container.createEllipse({cx:d.cx+shx,cy:d.cy+shy,rx:d.rx-sh,ry:d.ry-sh,r:r}).setStroke({width:_15,color:c});
}
},createForRect:function(o,_16,_17,d,r,p,c){
var sh=_16*_17/2,shy=/B/.test(p)?sh:/T/.test(p)?0:sh/2,shx=/R/.test(p)?sh:/L/.test(p)?0:sh/2;
for(var i=1;i<=_16;i++){
var _18=i*_17;
this.container.createRect({x:d.x+shx,y:d.y+shy,width:d.width-sh,height:d.height-sh,r:r}).setStroke({width:_18,color:c});
}
},arrowPoints:function(){
var d=this.stencil.data;
var _19=this.stencil.getRadius();
var _1a=this.style.zAngle+30;
var pt=this.util.pointOnCircle(d.x1,d.y1,_19*0.75,_1a);
var obj={start:{x:d.x1,y:d.y1},x:pt.x,y:pt.y};
var _1a=this.util.angle(obj);
var _1b=this.util.length(obj);
var al=this.style.arrows.length;
var aw=this.style.arrows.width/3;
if(_1b<al){
al=_1b/2;
}
var p1=this.util.pointOnCircle(obj.x,obj.y,-al,_1a-aw);
var p2=this.util.pointOnCircle(obj.x,obj.y,-al,_1a+aw);
return [{x:obj.x,y:obj.y},p1,p2];
},createForZArrow:function(o,_1c,_1d,pts,r,p,c){
if(this.stencil.data.cosphi<1||!this.stencil.points[0]){
return;
}
var sh=_1c*_1d/4,shy=/B/.test(p)?sh:/T/.test(p)?sh*-1:0,shx=/R/.test(p)?sh:/L/.test(p)?sh*-1:0;
var _1e=true;
for(var i=1;i<=_1c;i++){
var _1f=i*_1d;
pts=this.arrowPoints();
if(!pts){
return;
}
if(_3.gfx.renderer=="svg"){
var _20=[];
_2.forEach(pts,function(o,i){
if(i==0){
_20.push("M "+(o.x+shx)+" "+(o.y+shy));
}else{
var cmd=o.t||"L ";
_20.push(cmd+(o.x+shx)+" "+(o.y+shy));
}
},this);
if(_1e){
_20.push("Z");
}
this.container.createPath(_20.join(", ")).setStroke({width:_1f,color:c,cap:"round"}).setFill(c);
}else{
var pth=this.container.createPath({}).setStroke({width:_1f,color:c,cap:"round"});
_2.forEach(pts,function(o,i){
if(i==0||o.t=="M"){
pth.moveTo(o.x+shx,o.y+shy);
}else{
if(o.t=="Z"){
_1e&&pth.closePath();
}else{
pth.lineTo(o.x+shx,o.y+shy);
}
}
},this);
_1e&&pth.closePath();
}
var sp=this.stencil.points;
this.container.createLine({x1:sp[0].x,y1:sp[0].y,x2:pts[0].x,y2:pts[0].y}).setStroke({width:_1f,color:c,cap:"round"});
}
},onTransform:function(){
this.render();
},onRender:function(){
this.container.moveToBack();
},destroy:function(){
if(this.container){
this.container.removeShape();
}
}});
});
