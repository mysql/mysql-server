//>>built
define("dojox/gfx/canvas",["./_base","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-geometry","dojo/dom","./_base","./shape","./path","./arc","./matrix","./decompose"],function(g,_1,_2,_3,_4,_5,_6,_7,gs,_8,ga,m,_9){
var _a=g.canvas={};
var _b=null,mp=m.multiplyPoint,pi=Math.PI,_c=2*pi,_d=pi/2,_e=_1.extend;
_3("dojox.gfx.canvas.Shape",gs.Shape,{_render:function(_f){
_f.save();
this._renderTransform(_f);
this._renderShape(_f);
this._renderFill(_f,true);
this._renderStroke(_f,true);
_f.restore();
},_renderTransform:function(ctx){
if("canvasTransform" in this){
var t=this.canvasTransform;
ctx.translate(t.dx,t.dy);
ctx.rotate(t.angle2);
ctx.scale(t.sx,t.sy);
ctx.rotate(t.angle1);
}
},_renderShape:function(ctx){
},_renderFill:function(ctx,_10){
if("canvasFill" in this){
var fs=this.fillStyle;
if("canvasFillImage" in this){
var w=fs.width,h=fs.height,iw=this.canvasFillImage.width,ih=this.canvasFillImage.height,sx=w==iw?1:w/iw,sy=h==ih?1:h/ih,s=Math.min(sx,sy),dx=(w-s*iw)/2,dy=(h-s*ih)/2;
_b.width=w;
_b.height=h;
var _11=_b.getContext("2d");
_11.clearRect(0,0,w,h);
_11.drawImage(this.canvasFillImage,0,0,iw,ih,dx,dy,s*iw,s*ih);
this.canvasFill=ctx.createPattern(_b,"repeat");
delete this.canvasFillImage;
}
ctx.fillStyle=this.canvasFill;
if(_10){
if(fs.type==="pattern"&&(fs.x!==0||fs.y!==0)){
ctx.translate(fs.x,fs.y);
}
ctx.fill();
}
}else{
ctx.fillStyle="rgba(0,0,0,0.0)";
}
},_renderStroke:function(ctx,_12){
var s=this.strokeStyle;
if(s){
ctx.strokeStyle=s.color.toString();
ctx.lineWidth=s.width;
ctx.lineCap=s.cap;
if(typeof s.join=="number"){
ctx.lineJoin="miter";
ctx.miterLimit=s.join;
}else{
ctx.lineJoin=s.join;
}
if(_12){
ctx.stroke();
}
}else{
if(!_12){
ctx.strokeStyle="rgba(0,0,0,0.0)";
}
}
},getEventSource:function(){
return null;
},connect:function(){
},disconnect:function(){
}});
var _13=function(_14,_15,_16){
var old=_14.prototype[_15];
_14.prototype[_15]=_16?function(){
this.surface.makeDirty();
old.apply(this,arguments);
_16.call(this);
return this;
}:function(){
this.surface.makeDirty();
return old.apply(this,arguments);
};
};
_13(_a.Shape,"setTransform",function(){
if(this.matrix){
this.canvasTransform=g.decompose(this.matrix);
}else{
delete this.canvasTransform;
}
});
_13(_a.Shape,"setFill",function(){
var fs=this.fillStyle,f;
if(fs){
if(typeof (fs)=="object"&&"type" in fs){
var ctx=this.surface.rawNode.getContext("2d");
switch(fs.type){
case "linear":
case "radial":
f=fs.type=="linear"?ctx.createLinearGradient(fs.x1,fs.y1,fs.x2,fs.y2):ctx.createRadialGradient(fs.cx,fs.cy,0,fs.cx,fs.cy,fs.r);
_2.forEach(fs.colors,function(_17){
f.addColorStop(_17.offset,g.normalizeColor(_17.color).toString());
});
break;
case "pattern":
if(!_b){
_b=document.createElement("canvas");
}
var img=new Image();
this.surface.downloadImage(img,fs.src);
this.canvasFillImage=img;
}
}else{
f=fs.toString();
}
this.canvasFill=f;
}else{
delete this.canvasFill;
}
});
_13(_a.Shape,"setStroke");
_13(_a.Shape,"setShape");
_3("dojox.gfx.canvas.Group",_a.Shape,{constructor:function(){
gs.Container._init.call(this);
},_render:function(ctx){
ctx.save();
this._renderTransform(ctx);
for(var i=0;i<this.children.length;++i){
this.children[i]._render(ctx);
}
ctx.restore();
}});
_3("dojox.gfx.canvas.Rect",[_a.Shape,gs.Rect],{_renderShape:function(ctx){
var s=this.shape,r=Math.min(s.r,s.height/2,s.width/2),xl=s.x,xr=xl+s.width,yt=s.y,yb=yt+s.height,xl2=xl+r,xr2=xr-r,yt2=yt+r,yb2=yb-r;
ctx.beginPath();
ctx.moveTo(xl2,yt);
if(r){
ctx.arc(xr2,yt2,r,-_d,0,false);
ctx.arc(xr2,yb2,r,0,_d,false);
ctx.arc(xl2,yb2,r,_d,pi,false);
ctx.arc(xl2,yt2,r,pi,pi+_d,false);
}else{
ctx.lineTo(xr2,yt);
ctx.lineTo(xr,yb2);
ctx.lineTo(xl2,yb);
ctx.lineTo(xl,yt2);
}
ctx.closePath();
}});
var _18=[];
(function(){
var u=ga.curvePI4;
_18.push(u.s,u.c1,u.c2,u.e);
for(var a=45;a<360;a+=45){
var r=m.rotateg(a);
_18.push(mp(r,u.c1),mp(r,u.c2),mp(r,u.e));
}
})();
_3("dojox.gfx.canvas.Ellipse",[_a.Shape,gs.Ellipse],{setShape:function(){
this.inherited(arguments);
var s=this.shape,t,c1,c2,r=[],M=m.normalize([m.translate(s.cx,s.cy),m.scale(s.rx,s.ry)]);
t=mp(M,_18[0]);
r.push([t.x,t.y]);
for(var i=1;i<_18.length;i+=3){
c1=mp(M,_18[i]);
c2=mp(M,_18[i+1]);
t=mp(M,_18[i+2]);
r.push([c1.x,c1.y,c2.x,c2.y,t.x,t.y]);
}
this.canvasEllipse=r;
return this;
},_renderShape:function(ctx){
var r=this.canvasEllipse;
ctx.beginPath();
ctx.moveTo.apply(ctx,r[0]);
for(var i=1;i<r.length;++i){
ctx.bezierCurveTo.apply(ctx,r[i]);
}
ctx.closePath();
}});
_3("dojox.gfx.canvas.Circle",[_a.Shape,gs.Circle],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.arc(s.cx,s.cy,s.r,0,_c,1);
}});
_3("dojox.gfx.canvas.Line",[_a.Shape,gs.Line],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.moveTo(s.x1,s.y1);
ctx.lineTo(s.x2,s.y2);
}});
_3("dojox.gfx.canvas.Polyline",[_a.Shape,gs.Polyline],{setShape:function(){
this.inherited(arguments);
var p=this.shape.points,f=p[0],r,c,i;
this.bbox=null;
this._normalizePoints();
if(p.length){
if(typeof f=="number"){
r=p;
}else{
r=[];
for(i=0;i<p.length;++i){
c=p[i];
r.push(c.x,c.y);
}
}
}else{
r=[];
}
this.canvasPolyline=r;
return this;
},_renderShape:function(ctx){
var p=this.canvasPolyline;
if(p.length){
ctx.beginPath();
ctx.moveTo(p[0],p[1]);
for(var i=2;i<p.length;i+=2){
ctx.lineTo(p[i],p[i+1]);
}
}
}});
_3("dojox.gfx.canvas.Image",[_a.Shape,gs.Image],{setShape:function(){
this.inherited(arguments);
var img=new Image();
this.surface.downloadImage(img,this.shape.src);
this.canvasImage=img;
return this;
},_renderShape:function(ctx){
var s=this.shape;
ctx.drawImage(this.canvasImage,s.x,s.y,s.width,s.height);
}});
_3("dojox.gfx.canvas.Text",[_a.Shape,gs.Text],{_setFont:function(){
if(this.fontStyle){
this.canvasFont=g.makeFontString(this.fontStyle);
}else{
delete this.canvasFont;
}
},getTextWidth:function(){
var s=this.shape,w=0,ctx;
if(s.text&&s.text.length>0){
ctx=this.surface.rawNode.getContext("2d");
ctx.save();
this._renderTransform(ctx);
this._renderFill(ctx,false);
this._renderStroke(ctx,false);
if(this.canvasFont){
ctx.font=this.canvasFont;
}
w=ctx.measureText(s.text).width;
ctx.restore();
}
return w;
},_render:function(ctx){
ctx.save();
this._renderTransform(ctx);
this._renderFill(ctx,false);
this._renderStroke(ctx,false);
this._renderShape(ctx);
ctx.restore();
},_renderShape:function(ctx){
var ta,s=this.shape;
if(!s.text||s.text.length==0){
return;
}
ta=s.align==="middle"?"center":s.align;
ctx.textAlign=ta;
if(this.canvasFont){
ctx.font=this.canvasFont;
}
if(this.canvasFill){
ctx.fillText(s.text,s.x,s.y);
}
if(this.strokeStyle){
ctx.beginPath();
ctx.strokeText(s.text,s.x,s.y);
ctx.closePath();
}
}});
_13(_a.Text,"setFont");
if(_4.global.CanvasRenderingContext2D){
var _19=_4.doc.createElement("canvas").getContext("2d");
if(_19&&typeof _19.fillText!="function"){
_a.Text.extend({getTextWidth:function(){
return 0;
},_renderShape:function(){
}});
}
}
var _1a={M:"_moveToA",m:"_moveToR",L:"_lineToA",l:"_lineToR",H:"_hLineToA",h:"_hLineToR",V:"_vLineToA",v:"_vLineToR",C:"_curveToA",c:"_curveToR",S:"_smoothCurveToA",s:"_smoothCurveToR",Q:"_qCurveToA",q:"_qCurveToR",T:"_qSmoothCurveToA",t:"_qSmoothCurveToR",A:"_arcTo",a:"_arcTo",Z:"_closePath",z:"_closePath"};
_3("dojox.gfx.canvas.Path",[_a.Shape,_8.Path],{constructor:function(){
this.lastControl={};
},setShape:function(){
this.canvasPath=[];
return this.inherited(arguments);
},_updateWithSegment:function(_1b){
var _1c=_1.clone(this.last);
this[_1a[_1b.action]](this.canvasPath,_1b.action,_1b.args);
this.last=_1c;
this.inherited(arguments);
},_renderShape:function(ctx){
var r=this.canvasPath;
ctx.beginPath();
for(var i=0;i<r.length;i+=2){
ctx[r[i]].apply(ctx,r[i+1]);
}
},_moveToA:function(_1d,_1e,_1f){
_1d.push("moveTo",[_1f[0],_1f[1]]);
for(var i=2;i<_1f.length;i+=2){
_1d.push("lineTo",[_1f[i],_1f[i+1]]);
}
this.last.x=_1f[_1f.length-2];
this.last.y=_1f[_1f.length-1];
this.lastControl={};
},_moveToR:function(_20,_21,_22){
if("x" in this.last){
_20.push("moveTo",[this.last.x+=_22[0],this.last.y+=_22[1]]);
}else{
_20.push("moveTo",[this.last.x=_22[0],this.last.y=_22[1]]);
}
for(var i=2;i<_22.length;i+=2){
_20.push("lineTo",[this.last.x+=_22[i],this.last.y+=_22[i+1]]);
}
this.lastControl={};
},_lineToA:function(_23,_24,_25){
for(var i=0;i<_25.length;i+=2){
_23.push("lineTo",[_25[i],_25[i+1]]);
}
this.last.x=_25[_25.length-2];
this.last.y=_25[_25.length-1];
this.lastControl={};
},_lineToR:function(_26,_27,_28){
for(var i=0;i<_28.length;i+=2){
_26.push("lineTo",[this.last.x+=_28[i],this.last.y+=_28[i+1]]);
}
this.lastControl={};
},_hLineToA:function(_29,_2a,_2b){
for(var i=0;i<_2b.length;++i){
_29.push("lineTo",[_2b[i],this.last.y]);
}
this.last.x=_2b[_2b.length-1];
this.lastControl={};
},_hLineToR:function(_2c,_2d,_2e){
for(var i=0;i<_2e.length;++i){
_2c.push("lineTo",[this.last.x+=_2e[i],this.last.y]);
}
this.lastControl={};
},_vLineToA:function(_2f,_30,_31){
for(var i=0;i<_31.length;++i){
_2f.push("lineTo",[this.last.x,_31[i]]);
}
this.last.y=_31[_31.length-1];
this.lastControl={};
},_vLineToR:function(_32,_33,_34){
for(var i=0;i<_34.length;++i){
_32.push("lineTo",[this.last.x,this.last.y+=_34[i]]);
}
this.lastControl={};
},_curveToA:function(_35,_36,_37){
for(var i=0;i<_37.length;i+=6){
_35.push("bezierCurveTo",_37.slice(i,i+6));
}
this.last.x=_37[_37.length-2];
this.last.y=_37[_37.length-1];
this.lastControl.x=_37[_37.length-4];
this.lastControl.y=_37[_37.length-3];
this.lastControl.type="C";
},_curveToR:function(_38,_39,_3a){
for(var i=0;i<_3a.length;i+=6){
_38.push("bezierCurveTo",[this.last.x+_3a[i],this.last.y+_3a[i+1],this.lastControl.x=this.last.x+_3a[i+2],this.lastControl.y=this.last.y+_3a[i+3],this.last.x+_3a[i+4],this.last.y+_3a[i+5]]);
this.last.x+=_3a[i+4];
this.last.y+=_3a[i+5];
}
this.lastControl.type="C";
},_smoothCurveToA:function(_3b,_3c,_3d){
for(var i=0;i<_3d.length;i+=4){
var _3e=this.lastControl.type=="C";
_3b.push("bezierCurveTo",[_3e?2*this.last.x-this.lastControl.x:this.last.x,_3e?2*this.last.y-this.lastControl.y:this.last.y,_3d[i],_3d[i+1],_3d[i+2],_3d[i+3]]);
this.lastControl.x=_3d[i];
this.lastControl.y=_3d[i+1];
this.lastControl.type="C";
}
this.last.x=_3d[_3d.length-2];
this.last.y=_3d[_3d.length-1];
},_smoothCurveToR:function(_3f,_40,_41){
for(var i=0;i<_41.length;i+=4){
var _42=this.lastControl.type=="C";
_3f.push("bezierCurveTo",[_42?2*this.last.x-this.lastControl.x:this.last.x,_42?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_41[i],this.last.y+_41[i+1],this.last.x+_41[i+2],this.last.y+_41[i+3]]);
this.lastControl.x=this.last.x+_41[i];
this.lastControl.y=this.last.y+_41[i+1];
this.lastControl.type="C";
this.last.x+=_41[i+2];
this.last.y+=_41[i+3];
}
},_qCurveToA:function(_43,_44,_45){
for(var i=0;i<_45.length;i+=4){
_43.push("quadraticCurveTo",_45.slice(i,i+4));
}
this.last.x=_45[_45.length-2];
this.last.y=_45[_45.length-1];
this.lastControl.x=_45[_45.length-4];
this.lastControl.y=_45[_45.length-3];
this.lastControl.type="Q";
},_qCurveToR:function(_46,_47,_48){
for(var i=0;i<_48.length;i+=4){
_46.push("quadraticCurveTo",[this.lastControl.x=this.last.x+_48[i],this.lastControl.y=this.last.y+_48[i+1],this.last.x+_48[i+2],this.last.y+_48[i+3]]);
this.last.x+=_48[i+2];
this.last.y+=_48[i+3];
}
this.lastControl.type="Q";
},_qSmoothCurveToA:function(_49,_4a,_4b){
for(var i=0;i<_4b.length;i+=2){
var _4c=this.lastControl.type=="Q";
_49.push("quadraticCurveTo",[this.lastControl.x=_4c?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_4c?2*this.last.y-this.lastControl.y:this.last.y,_4b[i],_4b[i+1]]);
this.lastControl.type="Q";
}
this.last.x=_4b[_4b.length-2];
this.last.y=_4b[_4b.length-1];
},_qSmoothCurveToR:function(_4d,_4e,_4f){
for(var i=0;i<_4f.length;i+=2){
var _50=this.lastControl.type=="Q";
_4d.push("quadraticCurveTo",[this.lastControl.x=_50?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_50?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_4f[i],this.last.y+_4f[i+1]]);
this.lastControl.type="Q";
this.last.x+=_4f[i];
this.last.y+=_4f[i+1];
}
},_arcTo:function(_51,_52,_53){
var _54=_52=="a";
for(var i=0;i<_53.length;i+=7){
var x1=_53[i+5],y1=_53[i+6];
if(_54){
x1+=this.last.x;
y1+=this.last.y;
}
var _55=ga.arcAsBezier(this.last,_53[i],_53[i+1],_53[i+2],_53[i+3]?1:0,_53[i+4]?1:0,x1,y1);
_2.forEach(_55,function(p){
_51.push("bezierCurveTo",p);
});
this.last.x=x1;
this.last.y=y1;
}
this.lastControl={};
},_closePath:function(_56,_57,_58){
_56.push("closePath",[]);
this.lastControl={};
}});
_2.forEach(["moveTo","lineTo","hLineTo","vLineTo","curveTo","smoothCurveTo","qCurveTo","qSmoothCurveTo","arcTo","closePath"],function(_59){
_13(_a.Path,_59);
});
_3("dojox.gfx.canvas.TextPath",[_a.Shape,_8.TextPath],{_renderShape:function(ctx){
var s=this.shape;
},_setText:function(){
},_setFont:function(){
}});
_3("dojox.gfx.canvas.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
this.pendingImageCount=0;
this.makeDirty();
},setDimensions:function(_5a,_5b){
this.width=g.normalizedLength(_5a);
this.height=g.normalizedLength(_5b);
if(!this.rawNode){
return this;
}
var _5c=false;
if(this.rawNode.width!=this.width){
this.rawNode.width=this.width;
_5c=true;
}
if(this.rawNode.height!=this.height){
this.rawNode.height=this.height;
_5c=true;
}
if(_5c){
this.makeDirty();
}
return this;
},getDimensions:function(){
return this.rawNode?{width:this.rawNode.width,height:this.rawNode.height}:null;
},_render:function(){
if(this.pendingImageCount){
return;
}
var ctx=this.rawNode.getContext("2d");
ctx.save();
ctx.clearRect(0,0,this.rawNode.width,this.rawNode.height);
for(var i=0;i<this.children.length;++i){
this.children[i]._render(ctx);
}
ctx.restore();
if("pendingRender" in this){
clearTimeout(this.pendingRender);
delete this.pendingRender;
}
},makeDirty:function(){
if(!this.pendingImagesCount&&!("pendingRender" in this)){
this.pendingRender=setTimeout(_1.hitch(this,this._render),0);
}
},downloadImage:function(img,url){
var _5d=_1.hitch(this,this.onImageLoad);
if(!this.pendingImageCount++&&"pendingRender" in this){
clearTimeout(this.pendingRender);
delete this.pendingRender;
}
img.onload=_5d;
img.onerror=_5d;
img.onabort=_5d;
img.src=url;
},onImageLoad:function(){
if(!--this.pendingImageCount){
this._render();
}
},getEventSource:function(){
return null;
},connect:function(){
},disconnect:function(){
}});
_a.createSurface=function(_5e,_5f,_60){
if(!_5f&&!_60){
var pos=_5.position(_5e);
_5f=_5f||pos.w;
_60=_60||pos.h;
}
if(typeof _5f=="number"){
_5f=_5f+"px";
}
if(typeof _60=="number"){
_60=_60+"px";
}
var s=new _a.Surface(),p=_6.byId(_5e),c=p.ownerDocument.createElement("canvas");
c.width=g.normalizedLength(_5f);
c.height=g.normalizedLength(_60);
p.appendChild(c);
s.rawNode=c;
s._parent=p;
s.surface=s;
return s;
};
var C=gs.Container,_61={add:function(_62){
this.surface.makeDirty();
return C.add.apply(this,arguments);
},remove:function(_63,_64){
this.surface.makeDirty();
return C.remove.apply(this,arguments);
},clear:function(){
this.surface.makeDirty();
return C.clear.apply(this,arguments);
},_moveChildToFront:function(_65){
this.surface.makeDirty();
return C._moveChildToFront.apply(this,arguments);
},_moveChildToBack:function(_66){
this.surface.makeDirty();
return C._moveChildToBack.apply(this,arguments);
}};
var _67={createObject:function(_68,_69){
var _6a=new _68();
_6a.surface=this.surface;
_6a.setShape(_69);
this.add(_6a);
return _6a;
}};
_e(_a.Group,_61);
_e(_a.Group,gs.Creator);
_e(_a.Group,_67);
_e(_a.Surface,_61);
_e(_a.Surface,gs.Creator);
_e(_a.Surface,_67);
_a.fixTarget=function(_6b,_6c){
return true;
};
return _a;
});
