//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/fx/easing"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil._Base");
_2.require("dojo.fx.easing");
_3.drawing.stencil._Base=_3.drawing.util.oo.declare(function(_4){
_2.mixin(this,_4);
this.style=_4.style||_3.drawing.defaults.copy();
if(_4.stencil){
this.stencil=_4.stencil;
this.util=_4.stencil.util;
this.mouse=_4.stencil.mouse;
this.container=_4.stencil.container;
this.style=_4.stencil.style;
}
var _5=/Line|Vector|Axes|Arrow/;
var _6=/Text/;
this.shortType=this.util.abbr(this.type);
this.isText=_6.test(this.type);
this.isLine=_5.test(this.type);
this.renderHit=this.style.renderHitLayer;
if(!this.renderHit&&this.style.renderHitLines&&this.isLine){
this.renderHit=true;
}
if(!this.renderHit&&this.style.useSelectedStyle){
this.useSelectedStyle=true;
this.selCopy=_2.clone(this.style.selected);
for(var nm in this.style.norm){
if(this.style.selected[nm]===undefined){
this.style.selected[nm]=this.style.norm[nm];
}
}
this.textSelected=_2.clone(this.style.text);
this.textSelected.color=this.style.selected.fill;
}
this.angleSnap=this.style.angleSnap||1;
this.marginZero=_4.marginZero||this.style.anchors.marginZero;
this.id=_4.id||this.util.uid(this.type);
this._cons=[];
if(!this.annotation&&!this.subShape){
this.util.attr(this.container,"id",this.id);
}
this.connect(this,"onBeforeRender","preventNegativePos");
this._offX=this.mouse.origin.x;
this._offY=this.mouse.origin.y;
if(this.isText){
this.align=_4.align||this.align;
this.valign=_4.valign||this.valign;
if(_4.data&&_4.data.makeFit){
var _7=this.makeFit(_4.data.text,_4.data.width);
this.textSize=this.style.text.size=_7.size;
this._lineHeight=_7.box.h;
}else{
this.textSize=parseInt(this.style.text.size,10);
this._lineHeight=this.textSize*1.4;
}
this.deleteEmptyCreate=_4.deleteEmptyCreate!==undefined?_4.deleteEmptyCreate:this.style.text.deleteEmptyCreate;
this.deleteEmptyModify=_4.deleteEmptyModify!==undefined?_4.deleteEmptyModify:this.style.text.deleteEmptyModify;
}
this.attr(_4.data);
if(this.noBaseRender){
return;
}
if(_4.points){
if(_4.data&&_4.data.closePath===false){
this.closePath=false;
}
this.setPoints(_4.points);
this.connect(this,"render",this,"onRender",true);
this.baseRender&&this.enabled&&this.render();
_4.label&&this.setLabel(_4.label);
_4.shadow&&this.addShadow(_4.shadow);
}else{
if(_4.data){
_4.data.width=_4.data.width?_4.data.width:this.style.text.minWidth;
_4.data.height=_4.data.height?_4.data.height:this._lineHeight;
this.setData(_4.data);
this.connect(this,"render",this,"onRender",true);
this.baseRender&&this.enabled&&this.render(_4.data.text);
this.baseRender&&_4.label&&this.setLabel(_4.label);
this.baseRender&&_4.shadow&&this.addShadow(_4.shadow);
}else{
if(this.draws){
this.points=[];
this.data={};
this.connectMouse();
this._postRenderCon=_2.connect(this,"render",this,"_onPostRender");
}
}
}
if(this.showAngle){
this.angleLabel=new _3.drawing.annotations.Angle({stencil:this});
}
if(!this.enabled){
this.disable();
this.moveToBack();
this.render(_4.data.text);
}
},{type:"dojox.drawing.stencil",minimumSize:10,enabled:true,drawingType:"stencil",setData:function(_8){
this.data=_8;
this.points=this.dataToPoints();
},setPoints:function(_9){
this.points=_9;
if(this.pointsToData){
this.data=this.pointsToData();
}
},onDelete:function(_a){
},onBeforeRender:function(_b){
},onModify:function(_c){
},onChangeData:function(_d){
},onChangeText:function(_e){
},onRender:function(_f){
this._postRenderCon=_2.connect(this,"render",this,"_onPostRender");
this.created=true;
this.disconnectMouse();
if(this.shape){
this.shape.superClass=this;
}else{
this.container.superClass=this;
}
this._setNodeAtts(this);
},onChangeStyle:function(_10){
this._isBeingModified=true;
if(!this.enabled){
this.style.current=this.style.disabled;
this.style.currentText=this.style.textDisabled;
this.style.currentHit=this.style.hitNorm;
}else{
this.style.current=this.style.norm;
this.style.currentHit=this.style.hitNorm;
this.style.currentText=this.style.text;
}
if(this.selected){
if(this.useSelectedStyle){
this.style.current=this.style.selected;
this.style.currentText=this.textSelected;
}
this.style.currentHit=this.style.hitSelected;
}else{
if(this.highlighted){
this.style.currentHit=this.style.hitHighlighted;
}
}
this.render();
},animate:function(_11,_12){
console.warn("ANIMATE..........................");
var d=_11.d||_11.duration||1000;
var ms=_11.ms||20;
var _13=_11.ease||_2.fx.easing.linear;
var _14=_11.steps;
var ts=new Date().getTime();
var w=100;
var cnt=0;
var _15=true;
var sp,ep;
if(_2.isArray(_11.start)){
sp=_11.start;
ep=_11.end;
}else{
if(_2.isObject(_11.start)){
sp=_11.start;
ep=_11.end;
_15=false;
}else{
console.warn("No data provided to animate");
}
}
var v=setInterval(_2.hitch(this,function(){
var t=new Date().getTime()-ts;
var p=_13(1-t/d);
if(t>d||cnt++>100){
clearInterval(v);
return;
}
if(_15){
var _16=[];
_2.forEach(sp,function(pt,i){
var o={x:(ep[i].x-sp[i].x)*p+sp[i].x,y:(ep[i].y-sp[i].y)*p+sp[i].y};
_16.push(o);
});
this.setPoints(_16);
this.render();
}else{
var o={};
for(var nm in sp){
o[nm]=(ep[nm]-sp[nm])*p+sp[nm];
}
this.attr(o);
}
}),ms);
},attr:function(key,_17){
var n=this.enabled?this.style.norm:this.style.disabled;
var t=this.enabled?this.style.text:this.style.textDisabled;
var ts=this.textSelected||{},o,nm,_18,_19=_2.toJson(n),_1a=_2.toJson(t);
var _1b={x:true,y:true,r:true,height:true,width:true,radius:true,angle:true};
var _1c=false;
if(typeof (key)!="object"){
o={};
o[key]=_17;
}else{
o=_2.clone(key);
}
if(o.width){
_18=o.width;
delete o.width;
}
for(nm in o){
if(nm in n){
n[nm]=o[nm];
}
if(nm in t){
t[nm]=o[nm];
}
if(nm in ts){
ts[nm]=o[nm];
}
if(nm in _1b){
_1b[nm]=o[nm];
_1c=true;
if(nm=="radius"&&o.angle===undefined){
o.angle=_1b.angle=this.getAngle();
}else{
if(nm=="angle"&&o.radius===undefined){
o.radius=_1b.radius=this.getRadius();
}
}
}
if(nm=="text"){
this.setText(o.text);
}
if(nm=="label"){
this.setLabel(o.label);
}
}
if(o.borderWidth!==undefined){
n.width=o.borderWidth;
}
if(this.useSelectedStyle){
for(nm in this.style.norm){
if(this.selCopy[nm]===undefined){
this.style.selected[nm]=this.style.norm[nm];
}
}
this.textSelected.color=this.style.selected.color;
}
if(!this.created){
return;
}
if(o.x!==undefined||o.y!==undefined){
var box=this.getBounds(true);
var mx={dx:0,dy:0};
for(nm in o){
if(nm=="x"||nm=="y"||nm=="r"){
mx["d"+nm]=o[nm]-box[nm];
}
}
this.transformPoints(mx);
}
var p=this.points;
if(o.angle!==undefined){
this.dataToPoints({x:this.data.x1,y:this.data.y1,angle:o.angle,radius:o.radius});
}else{
if(_18!==undefined){
p[1].x=p[2].x=p[0].x+_18;
this.pointsToData(p);
}
}
if(o.height!==undefined&&o.angle===undefined){
p[2].y=p[3].y=p[0].y+o.height;
this.pointsToData(p);
}
if(o.r!==undefined){
this.data.r=Math.max(0,o.r);
}
if(_1c||_1a!=_2.toJson(t)||_19!=_2.toJson(n)){
this.onChangeStyle(this);
}
o.width=_18;
if(o.cosphi!=undefined){
!this.data?this.data={cosphi:o.cosphi}:this.data.cosphi=o.cosphi;
this.style.zAxis=o.cosphi!=0?true:false;
}
},exporter:function(){
var _1d=this.type.substring(this.type.lastIndexOf(".")+1).charAt(0).toLowerCase()+this.type.substring(this.type.lastIndexOf(".")+2);
var o=_2.clone(this.style.norm);
o.borderWidth=o.width;
delete o.width;
if(_1d=="path"){
o.points=this.points;
}else{
o=_2.mixin(o,this.data);
}
o.type=_1d;
if(this.isText){
o.text=this.getText();
o=_2.mixin(o,this.style.text);
delete o.minWidth;
delete o.deleteEmptyCreate;
delete o.deleteEmptyModify;
}
var lbl=this.getLabel();
if(lbl){
o.label=lbl;
}
return o;
},disable:function(){
this.enabled=false;
this.renderHit=false;
this.onChangeStyle(this);
},enable:function(){
this.enabled=true;
this.renderHit=true;
this.onChangeStyle(this);
},select:function(){
this.selected=true;
this.onChangeStyle(this);
},deselect:function(_1e){
if(_1e){
setTimeout(_2.hitch(this,function(){
this.selected=false;
this.onChangeStyle(this);
}),200);
}else{
this.selected=false;
this.onChangeStyle(this);
}
},_toggleSelected:function(){
if(!this.selected){
return;
}
this.deselect();
setTimeout(_2.hitch(this,"select"),0);
},highlight:function(){
this.highlighted=true;
this.onChangeStyle(this);
},unhighlight:function(){
this.highlighted=false;
this.onChangeStyle(this);
},moveToFront:function(){
this.container&&this.container.moveToFront();
},moveToBack:function(){
this.container&&this.container.moveToBack();
},onTransformBegin:function(_1f){
this._isBeingModified=true;
},onTransformEnd:function(_20){
this._isBeingModified=false;
this.onModify(this);
},onTransform:function(_21){
if(!this._isBeingModified){
this.onTransformBegin();
}
this.setPoints(this.points);
this.render();
},transformPoints:function(mx){
if(!mx.dx&&!mx.dy){
return;
}
var _22=_2.clone(this.points),_23=false;
_2.forEach(this.points,function(o){
o.x+=mx.dx;
o.y+=mx.dy;
if(o.x<this.marginZero||o.y<this.marginZero){
_23=true;
}
});
if(_23){
this.points=_22;
console.error("Attempt to set object '"+this.id+"' to less than zero.");
return;
}
this.onTransform();
this.onTransformEnd();
},applyTransform:function(mx){
this.transformPoints(mx);
},setTransform:function(mx){
this.attr({x:mx.dx,y:mx.dy});
},getTransform:function(){
return this.selected?this.container.getParent().getTransform():{dx:0,dy:0};
},addShadow:function(_24){
_24=_24===true?{}:_24;
_24.stencil=this;
this.shadow=new _3.drawing.annotations.BoxShadow(_24);
},removeShadow:function(){
this.shadow.destroy();
},setLabel:function(_25){
if(!this._label){
this._label=new _3.drawing.annotations.Label({text:_25,util:this.util,mouse:this.mouse,stencil:this,annotation:true,container:this.container,labelPosition:this.labelPosition});
}else{
if(_25!=undefined){
this._label.setLabel(_25);
}
}
},getLabel:function(){
if(this._label){
return this._label.getText();
}
return null;
},getAngle:function(){
var d=this.pointsToData();
var obj={start:{x:d.x1,y:d.y1},x:d.x2,y:d.y2};
var _26=this.util.angle(obj,this.angleSnap);
_26<0?_26=360+_26:_26;
return _26;
},getRadius:function(){
var box=this.getBounds(true);
var _27={start:{x:box.x1,y:box.y1},x:box.x2,y:box.y2};
return this.util.length(_27);
},getBounds:function(_28){
var p=this.points,x1,y1,x2,y2;
if(p.length==2){
if(_28){
x1=p[0].x;
y1=p[0].y;
x2=p[1].x;
y2=p[1].y;
}else{
x1=p[0].x<p[1].x?p[0].x:p[1].x;
y1=p[0].y<p[1].y?p[0].y:p[1].y;
x2=p[0].x<p[1].x?p[1].x:p[0].x;
y2=p[0].y<p[1].y?p[1].y:p[0].y;
}
return {x1:x1,y1:y1,x2:x2,y2:y2,x:x1,y:y1,w:x2-x1,h:y2-y1};
}else{
return {x1:p[0].x,y1:p[0].y,x2:p[2].x,y2:p[2].y,x:p[0].x,y:p[0].y,w:p[2].x-p[0].x,h:p[2].y-p[0].y};
}
},preventNegativePos:function(){
if(this._isBeingModified){
return;
}
if(!this.points||!this.points.length){
return;
}
if(this.type=="dojox.drawing.tools.custom.Axes"){
var _29=this.marginZero,_2a=this.marginZero;
_2.forEach(this.points,function(p){
_29=Math.min(p.y,_29);
});
_2.forEach(this.points,function(p){
_2a=Math.min(p.x,_2a);
});
if(_29<this.marginZero){
_2.forEach(this.points,function(p,i){
p.y=p.y+(this.marginZero-_29);
},this);
}
if(_2a<this.marginZero){
_2.forEach(this.points,function(p){
p.x+=(this.marginZero-_2a);
},this);
}
}else{
_2.forEach(this.points,function(p){
p.x=p.x<0?this.marginZero:p.x;
p.y=p.y<0?this.marginZero:p.y;
});
}
this.setPoints(this.points);
},_onPostRender:function(_2b){
if(this._isBeingModified){
this.onModify(this);
this._isBeingModified=false;
}else{
if(!this.created){
}
}
if(!this.editMode&&!this.selected&&this._prevData&&_2.toJson(this._prevData)!=_2.toJson(this.data)){
this.onChangeData(this);
this._prevData=_2.clone(this.data);
}else{
if(!this._prevData&&(!this.isText||this.getText())){
this._prevData=_2.clone(this.data);
}
}
},_setNodeAtts:function(_2c){
var att=this.enabled&&(!this.annotation||this.drawingType=="label")?this.drawingType:"";
this.util.attr(_2c,"drawingType",att);
},destroy:function(){
if(this.destroyed){
return;
}
if(this.data||this.points&&this.points.length){
this.onDelete(this);
}
this.disconnectMouse();
this.disconnect(this._cons);
_2.disconnect(this._postRenderCon);
this.remove(this.shape,this.hit);
this.destroyed=true;
},remove:function(){
var a=arguments;
if(!a.length){
if(!this.shape){
return;
}
a=[this.shape];
}
for(var i=0;i<a.length;i++){
if(a[i]){
a[i].removeShape();
}
}
},connectMult:function(){
if(arguments.length>1){
this._cons.push(this.connect.apply(this,arguments));
}else{
if(_2.isArray(arguments[0][0])){
_2.forEach(arguments[0],function(ar){
this._cons.push(this.connect.apply(this,ar));
},this);
}else{
this._cons.push(this.connect.apply(this,arguments[0]));
}
}
},connect:function(o,e,s,m,_2d){
var c;
if(typeof (o)!="object"){
if(s){
m=s;
s=e;
e=o;
o=this;
}else{
m=e;
e=o;
o=s=this;
}
}else{
if(!m){
m=s;
s=this;
}else{
if(_2d){
c=_2.connect(o,e,function(evt){
_2.hitch(s,m)(evt);
_2.disconnect(c);
});
this._cons.push(c);
return c;
}else{
}
}
}
c=_2.connect(o,e,s,m);
this._cons.push(c);
return c;
},disconnect:function(_2e){
if(!_2e){
return;
}
if(!_2.isArray(_2e)){
_2e=[_2e];
}
_2.forEach(_2e,_2.disconnect,_2);
},connectMouse:function(){
this._mouseHandle=this.mouse.register(this);
},disconnectMouse:function(){
this.mouse.unregister(this._mouseHandle);
},render:function(){
},dataToPoints:function(_2f){
},pointsToData:function(_30){
},onDown:function(obj){
this._downOnCanvas=true;
_2.disconnect(this._postRenderCon);
this._postRenderCon=null;
},onMove:function(obj){
},onDrag:function(obj){
},onUp:function(obj){
}});
});
