//>>built
define("dojox/drawing/stencil/_Base",["dojo","dojo/fx/easing","../util/oo","../annotations/BoxShadow","../annotations/Angle","../annotations/Label","../defaults"],function(_1,_2,oo,_3,_4,_5,_6){
var _7=oo.declare(function(_8){
_1.mixin(this,_8);
this.style=_8.style||_6.copy();
if(_8.stencil){
this.stencil=_8.stencil;
this.util=_8.stencil.util;
this.mouse=_8.stencil.mouse;
this.container=_8.stencil.container;
this.style=_8.stencil.style;
}
var _9=/Line|Vector|Axes|Arrow/;
var _a=/Text/;
this.shortType=this.util.abbr(this.type);
this.isText=_a.test(this.type);
this.isLine=_9.test(this.type);
this.renderHit=this.style.renderHitLayer;
if(!this.renderHit&&this.style.renderHitLines&&this.isLine){
this.renderHit=true;
}
if(!this.renderHit&&this.style.useSelectedStyle){
this.useSelectedStyle=true;
this.selCopy=_1.clone(this.style.selected);
for(var nm in this.style.norm){
if(this.style.selected[nm]===undefined){
this.style.selected[nm]=this.style.norm[nm];
}
}
this.textSelected=_1.clone(this.style.text);
this.textSelected.color=this.style.selected.fill;
}
this.angleSnap=this.style.angleSnap||1;
this.marginZero=_8.marginZero||this.style.anchors.marginZero;
this.id=_8.id||this.util.uid(this.type);
this._cons=[];
if(!this.annotation&&!this.subShape){
this.util.attr(this.container,"id",this.id);
}
this.connect(this,"onBeforeRender","preventNegativePos");
this._offX=this.mouse.origin.x;
this._offY=this.mouse.origin.y;
if(this.isText){
this.align=_8.align||this.align;
this.valign=_8.valign||this.valign;
if(_8.data&&_8.data.makeFit){
var _b=this.makeFit(_8.data.text,_8.data.width);
this.textSize=this.style.text.size=_b.size;
this._lineHeight=_b.box.h;
}else{
this.textSize=parseInt(this.style.text.size,10);
this._lineHeight=this.textSize*1.4;
}
this.deleteEmptyCreate=_8.deleteEmptyCreate!==undefined?_8.deleteEmptyCreate:this.style.text.deleteEmptyCreate;
this.deleteEmptyModify=_8.deleteEmptyModify!==undefined?_8.deleteEmptyModify:this.style.text.deleteEmptyModify;
}
this.attr(_8.data);
if(this.noBaseRender){
return;
}
if(_8.points){
if(_8.data&&_8.data.closePath===false){
this.closePath=false;
}
this.setPoints(_8.points);
this.connect(this,"render",this,"onRender",true);
this.baseRender&&this.enabled&&this.render();
_8.label&&this.setLabel(_8.label);
_8.shadow&&this.addShadow(_8.shadow);
}else{
if(_8.data){
_8.data.width=_8.data.width?_8.data.width:this.style.text.minWidth;
_8.data.height=_8.data.height?_8.data.height:this._lineHeight;
this.setData(_8.data);
this.connect(this,"render",this,"onRender",true);
this.baseRender&&this.enabled&&this.render(_8.data.text);
this.baseRender&&_8.label&&this.setLabel(_8.label);
this.baseRender&&_8.shadow&&this.addShadow(_8.shadow);
}else{
if(this.draws){
this.points=[];
this.data={};
this.connectMouse();
this._postRenderCon=_1.connect(this,"render",this,"_onPostRender");
}
}
}
if(this.showAngle){
this.angleLabel=new _4({stencil:this});
}
if(!this.enabled){
this.disable();
this.moveToBack();
this.render(_8.data.text);
}
},{type:"dojox.drawing.stencil",minimumSize:10,enabled:true,drawingType:"stencil",setData:function(_c){
this.data=_c;
this.points=this.dataToPoints();
},setPoints:function(_d){
this.points=_d;
if(this.pointsToData){
this.data=this.pointsToData();
}
},onDelete:function(_e){
},onBeforeRender:function(_f){
},onModify:function(_10){
},onChangeData:function(_11){
},onChangeText:function(_12){
},onRender:function(_13){
this._postRenderCon=_1.connect(this,"render",this,"_onPostRender");
this.created=true;
this.disconnectMouse();
if(this.shape){
this.shape.superClass=this;
}else{
this.container.superClass=this;
}
this._setNodeAtts(this);
},onChangeStyle:function(_14){
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
},animate:function(_15,_16){
console.warn("ANIMATE..........................");
var d=_15.d||_15.duration||1000;
var ms=_15.ms||20;
var _17=_15.ease||_2.linear;
var _18=_15.steps;
var ts=new Date().getTime();
var w=100;
var cnt=0;
var _19=true;
var sp,ep;
if(_1.isArray(_15.start)){
sp=_15.start;
ep=_15.end;
}else{
if(_1.isObject(_15.start)){
sp=_15.start;
ep=_15.end;
_19=false;
}else{
console.warn("No data provided to animate");
}
}
var v=setInterval(_1.hitch(this,function(){
var t=new Date().getTime()-ts;
var p=_17(1-t/d);
if(t>d||cnt++>100){
clearInterval(v);
return;
}
if(_19){
var _1a=[];
_1.forEach(sp,function(pt,i){
var o={x:(ep[i].x-sp[i].x)*p+sp[i].x,y:(ep[i].y-sp[i].y)*p+sp[i].y};
_1a.push(o);
});
this.setPoints(_1a);
this.render();
}else{
var o={};
for(var nm in sp){
o[nm]=(ep[nm]-sp[nm])*p+sp[nm];
}
this.attr(o);
}
}),ms);
},attr:function(key,_1b){
var n=this.enabled?this.style.norm:this.style.disabled;
var t=this.enabled?this.style.text:this.style.textDisabled;
var ts=this.textSelected||{},o,nm,_1c,_1d=_1.toJson(n),_1e=_1.toJson(t);
var _1f={x:true,y:true,r:true,height:true,width:true,radius:true,angle:true};
var _20=false;
if(typeof (key)!="object"){
o={};
o[key]=_1b;
}else{
o=_1.clone(key);
}
if(o.width){
_1c=o.width;
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
if(nm in _1f){
_1f[nm]=o[nm];
_20=true;
if(nm=="radius"&&o.angle===undefined){
o.angle=_1f.angle=this.getAngle();
}else{
if(nm=="angle"&&o.radius===undefined){
o.radius=_1f.radius=this.getRadius();
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
if(_1c!==undefined){
p[1].x=p[2].x=p[0].x+_1c;
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
if(_20||_1e!=_1.toJson(t)||_1d!=_1.toJson(n)){
this.onChangeStyle(this);
}
o.width=_1c;
if(o.cosphi!=undefined){
!this.data?this.data={cosphi:o.cosphi}:this.data.cosphi=o.cosphi;
this.style.zAxis=o.cosphi!=0?true:false;
}
},exporter:function(){
var _21=this.type.substring(this.type.lastIndexOf(".")+1).charAt(0).toLowerCase()+this.type.substring(this.type.lastIndexOf(".")+2);
var o=_1.clone(this.style.norm);
o.borderWidth=o.width;
delete o.width;
if(_21=="path"){
o.points=this.points;
}else{
o=_1.mixin(o,this.data);
}
o.type=_21;
if(this.isText){
o.text=this.getText();
o=_1.mixin(o,this.style.text);
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
},deselect:function(_22){
if(_22){
setTimeout(_1.hitch(this,function(){
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
setTimeout(_1.hitch(this,"select"),0);
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
},onTransformBegin:function(_23){
this._isBeingModified=true;
},onTransformEnd:function(_24){
this._isBeingModified=false;
this.onModify(this);
},onTransform:function(_25){
if(!this._isBeingModified){
this.onTransformBegin();
}
this.setPoints(this.points);
this.render();
},transformPoints:function(mx){
if(!mx.dx&&!mx.dy){
return;
}
var _26=_1.clone(this.points),_27=false;
_1.forEach(this.points,function(o){
o.x+=mx.dx;
o.y+=mx.dy;
if(o.x<this.marginZero||o.y<this.marginZero){
_27=true;
}
});
if(_27){
this.points=_26;
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
},addShadow:function(_28){
_28=_28===true?{}:_28;
_28.stencil=this;
this.shadow=new _3(_28);
},removeShadow:function(){
this.shadow.destroy();
},setLabel:function(_29){
if(!this._label){
this._label=new _5.Label({text:_29,util:this.util,mouse:this.mouse,stencil:this,annotation:true,container:this.container,labelPosition:this.labelPosition});
}else{
if(_29!=undefined){
this._label.setLabel(_29);
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
var _2a=this.util.angle(obj,this.angleSnap);
_2a<0?_2a=360+_2a:_2a;
return _2a;
},getRadius:function(){
var box=this.getBounds(true);
var _2b={start:{x:box.x1,y:box.y1},x:box.x2,y:box.y2};
return this.util.length(_2b);
},getBounds:function(_2c){
var p=this.points,x1,y1,x2,y2;
if(p.length==2){
if(_2c){
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
var _2d=this.marginZero,_2e=this.marginZero;
_1.forEach(this.points,function(p){
_2d=Math.min(p.y,_2d);
});
_1.forEach(this.points,function(p){
_2e=Math.min(p.x,_2e);
});
if(_2d<this.marginZero){
_1.forEach(this.points,function(p,i){
p.y=p.y+(this.marginZero-_2d);
},this);
}
if(_2e<this.marginZero){
_1.forEach(this.points,function(p){
p.x+=(this.marginZero-_2e);
},this);
}
}else{
_1.forEach(this.points,function(p){
p.x=p.x<0?this.marginZero:p.x;
p.y=p.y<0?this.marginZero:p.y;
});
}
this.setPoints(this.points);
},_onPostRender:function(_2f){
if(this._isBeingModified){
this.onModify(this);
this._isBeingModified=false;
}else{
if(!this.created){
}
}
if(!this.editMode&&!this.selected&&this._prevData&&_1.toJson(this._prevData)!=_1.toJson(this.data)){
this.onChangeData(this);
this._prevData=_1.clone(this.data);
}else{
if(!this._prevData&&(!this.isText||this.getText())){
this._prevData=_1.clone(this.data);
}
}
},_setNodeAtts:function(_30){
var att=this.enabled&&(!this.annotation||this.drawingType=="label")?this.drawingType:"";
this.util.attr(_30,"drawingType",att);
},destroy:function(){
if(this.destroyed){
return;
}
if(this.data||this.points&&this.points.length){
this.onDelete(this);
}
this.disconnectMouse();
this.disconnect(this._cons);
_1.disconnect(this._postRenderCon);
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
if(_1.isArray(arguments[0][0])){
_1.forEach(arguments[0],function(ar){
this._cons.push(this.connect.apply(this,ar));
},this);
}else{
this._cons.push(this.connect.apply(this,arguments[0]));
}
}
},connect:function(o,e,s,m,_31){
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
if(_31){
c=_1.connect(o,e,function(evt){
_1.hitch(s,m)(evt);
_1.disconnect(c);
});
this._cons.push(c);
return c;
}else{
}
}
}
c=_1.connect(o,e,s,m);
this._cons.push(c);
return c;
},disconnect:function(_32){
if(!_32){
return;
}
if(!_1.isArray(_32)){
_32=[_32];
}
_1.forEach(_32,_1.disconnect,_1);
},connectMouse:function(){
this._mouseHandle=this.mouse.register(this);
},disconnectMouse:function(){
this.mouse.unregister(this._mouseHandle);
},render:function(){
},dataToPoints:function(_33){
},pointsToData:function(_34){
},onDown:function(obj){
this._downOnCanvas=true;
_1.disconnect(this._postRenderCon);
this._postRenderCon=null;
},onMove:function(obj){
},onDrag:function(obj){
},onUp:function(obj){
}});
_1.setObject("dojox.drawing.stencil._Base",_7);
return _7;
});
