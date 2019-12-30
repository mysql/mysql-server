//>>built
define("dojox/gfx/canvas",["./_base","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-geometry","dojo/dom","./_base","./shape","./path","./arc","./matrix","./decompose"],function(g,_1,_2,_3,_4,_5,_6,_7,gs,_8,ga,m,_9){
var _a=g.canvas={};
var _b=null,mp=m.multiplyPoint,pi=Math.PI,_c=2*pi,_d=pi/2,_e=_1.extend;
_a.Shape=_3("dojox.gfx.canvas.Shape",gs.Shape,{_render:function(_f){
_f.save();
this._renderTransform(_f);
this._renderClip(_f);
this._renderShape(_f);
this._renderFill(_f,true);
this._renderStroke(_f,true);
_f.restore();
},_renderClip:function(ctx){
if(this.canvasClip){
this.canvasClip.render(ctx);
ctx.clip();
}
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
},canvasClip:null,setClip:function(_13){
this.inherited(arguments);
var _14=_13?"width" in _13?"rect":"cx" in _13?"ellipse":"points" in _13?"polyline":"d" in _13?"path":null:null;
if(_13&&!_14){
return this;
}
this.canvasClip=_13?_15(_14,_13):null;
this.surface.makeDirty();
return this;
}});
var _15=function(_16,_17){
switch(_16){
case "ellipse":
return {canvasEllipse:_18(_17),render:function(ctx){
return _a.Ellipse.prototype._renderShape.call(this,ctx);
}};
case "rect":
return {shape:_1.delegate(_17,{r:0}),render:function(ctx){
return _a.Rect.prototype._renderShape.call(this,ctx);
}};
case "path":
return {canvasPath:_19(_17),render:function(ctx){
this.canvasPath._renderShape(ctx);
}};
case "polyline":
return {canvasPolyline:_17.points,render:function(ctx){
return _a.Polyline.prototype._renderShape.call(this,ctx);
}};
}
return null;
};
var _19=function(geo){
var p=new dojox.gfx.canvas.Path();
p.canvasPath=[];
p._setPath(geo.d);
return p;
};
var _1a=function(_1b,_1c,_1d){
var old=_1b.prototype[_1c];
_1b.prototype[_1c]=_1d?function(){
this.surface.makeDirty();
old.apply(this,arguments);
_1d.call(this);
return this;
}:function(){
this.surface.makeDirty();
return old.apply(this,arguments);
};
};
_1a(_a.Shape,"setTransform",function(){
if(this.matrix){
this.canvasTransform=g.decompose(this.matrix);
}else{
delete this.canvasTransform;
}
});
_1a(_a.Shape,"setFill",function(){
var fs=this.fillStyle,f;
if(fs){
if(typeof (fs)=="object"&&"type" in fs){
var ctx=this.surface.rawNode.getContext("2d");
switch(fs.type){
case "linear":
case "radial":
f=fs.type=="linear"?ctx.createLinearGradient(fs.x1,fs.y1,fs.x2,fs.y2):ctx.createRadialGradient(fs.cx,fs.cy,0,fs.cx,fs.cy,fs.r);
_2.forEach(fs.colors,function(_1e){
f.addColorStop(_1e.offset,g.normalizeColor(_1e.color).toString());
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
_1a(_a.Shape,"setStroke");
_1a(_a.Shape,"setShape");
_a.Group=_3("dojox.gfx.canvas.Group",_a.Shape,{constructor:function(){
gs.Container._init.call(this);
},_render:function(ctx){
ctx.save();
this._renderTransform(ctx);
this._renderClip(ctx);
for(var i=0;i<this.children.length;++i){
this.children[i]._render(ctx);
}
ctx.restore();
},destroy:function(){
this.clear(true);
_a.Shape.prototype.destroy.apply(this,arguments);
}});
_a.Rect=_3("dojox.gfx.canvas.Rect",[_a.Shape,gs.Rect],{_renderShape:function(ctx){
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
var _1f=[];
(function(){
var u=ga.curvePI4;
_1f.push(u.s,u.c1,u.c2,u.e);
for(var a=45;a<360;a+=45){
var r=m.rotateg(a);
_1f.push(mp(r,u.c1),mp(r,u.c2),mp(r,u.e));
}
})();
var _18=function(s){
var t,c1,c2,r=[],M=m.normalize([m.translate(s.cx,s.cy),m.scale(s.rx,s.ry)]);
t=mp(M,_1f[0]);
r.push([t.x,t.y]);
for(var i=1;i<_1f.length;i+=3){
c1=mp(M,_1f[i]);
c2=mp(M,_1f[i+1]);
t=mp(M,_1f[i+2]);
r.push([c1.x,c1.y,c2.x,c2.y,t.x,t.y]);
}
return r;
};
_a.Ellipse=_3("dojox.gfx.canvas.Ellipse",[_a.Shape,gs.Ellipse],{setShape:function(){
this.inherited(arguments);
this.canvasEllipse=_18(this.shape);
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
_a.Circle=_3("dojox.gfx.canvas.Circle",[_a.Shape,gs.Circle],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.arc(s.cx,s.cy,s.r,0,_c,1);
}});
_a.Line=_3("dojox.gfx.canvas.Line",[_a.Shape,gs.Line],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.moveTo(s.x1,s.y1);
ctx.lineTo(s.x2,s.y2);
}});
_a.Polyline=_3("dojox.gfx.canvas.Polyline",[_a.Shape,gs.Polyline],{setShape:function(){
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
_a.Image=_3("dojox.gfx.canvas.Image",[_a.Shape,gs.Image],{setShape:function(){
this.inherited(arguments);
var img=new Image();
this.surface.downloadImage(img,this.shape.src);
this.canvasImage=img;
return this;
},_renderShape:function(ctx){
var s=this.shape;
ctx.drawImage(this.canvasImage,s.x,s.y,s.width,s.height);
}});
_a.Text=_3("dojox.gfx.canvas.Text",[_a.Shape,gs.Text],{_setFont:function(){
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
_1a(_a.Text,"setFont");
if(_4.global.CanvasRenderingContext2D){
var _20=_4.doc.createElement("canvas").getContext("2d");
if(_20&&typeof _20.fillText!="function"){
_a.Text.extend({getTextWidth:function(){
return 0;
},_renderShape:function(){
}});
}
}
var _21={M:"_moveToA",m:"_moveToR",L:"_lineToA",l:"_lineToR",H:"_hLineToA",h:"_hLineToR",V:"_vLineToA",v:"_vLineToR",C:"_curveToA",c:"_curveToR",S:"_smoothCurveToA",s:"_smoothCurveToR",Q:"_qCurveToA",q:"_qCurveToR",T:"_qSmoothCurveToA",t:"_qSmoothCurveToR",A:"_arcTo",a:"_arcTo",Z:"_closePath",z:"_closePath"};
_a.Path=_3("dojox.gfx.canvas.Path",[_a.Shape,_8.Path],{constructor:function(){
this.lastControl={};
},setShape:function(){
this.canvasPath=[];
return this.inherited(arguments);
},_updateWithSegment:function(_22){
var _23=_1.clone(this.last);
this[_21[_22.action]](this.canvasPath,_22.action,_22.args);
this.last=_23;
this.inherited(arguments);
},_renderShape:function(ctx){
var r=this.canvasPath;
ctx.beginPath();
for(var i=0;i<r.length;i+=2){
ctx[r[i]].apply(ctx,r[i+1]);
}
},_moveToA:function(_24,_25,_26){
_24.push("moveTo",[_26[0],_26[1]]);
for(var i=2;i<_26.length;i+=2){
_24.push("lineTo",[_26[i],_26[i+1]]);
}
this.last.x=_26[_26.length-2];
this.last.y=_26[_26.length-1];
this.lastControl={};
},_moveToR:function(_27,_28,_29){
if("x" in this.last){
_27.push("moveTo",[this.last.x+=_29[0],this.last.y+=_29[1]]);
}else{
_27.push("moveTo",[this.last.x=_29[0],this.last.y=_29[1]]);
}
for(var i=2;i<_29.length;i+=2){
_27.push("lineTo",[this.last.x+=_29[i],this.last.y+=_29[i+1]]);
}
this.lastControl={};
},_lineToA:function(_2a,_2b,_2c){
for(var i=0;i<_2c.length;i+=2){
_2a.push("lineTo",[_2c[i],_2c[i+1]]);
}
this.last.x=_2c[_2c.length-2];
this.last.y=_2c[_2c.length-1];
this.lastControl={};
},_lineToR:function(_2d,_2e,_2f){
for(var i=0;i<_2f.length;i+=2){
_2d.push("lineTo",[this.last.x+=_2f[i],this.last.y+=_2f[i+1]]);
}
this.lastControl={};
},_hLineToA:function(_30,_31,_32){
for(var i=0;i<_32.length;++i){
_30.push("lineTo",[_32[i],this.last.y]);
}
this.last.x=_32[_32.length-1];
this.lastControl={};
},_hLineToR:function(_33,_34,_35){
for(var i=0;i<_35.length;++i){
_33.push("lineTo",[this.last.x+=_35[i],this.last.y]);
}
this.lastControl={};
},_vLineToA:function(_36,_37,_38){
for(var i=0;i<_38.length;++i){
_36.push("lineTo",[this.last.x,_38[i]]);
}
this.last.y=_38[_38.length-1];
this.lastControl={};
},_vLineToR:function(_39,_3a,_3b){
for(var i=0;i<_3b.length;++i){
_39.push("lineTo",[this.last.x,this.last.y+=_3b[i]]);
}
this.lastControl={};
},_curveToA:function(_3c,_3d,_3e){
for(var i=0;i<_3e.length;i+=6){
_3c.push("bezierCurveTo",_3e.slice(i,i+6));
}
this.last.x=_3e[_3e.length-2];
this.last.y=_3e[_3e.length-1];
this.lastControl.x=_3e[_3e.length-4];
this.lastControl.y=_3e[_3e.length-3];
this.lastControl.type="C";
},_curveToR:function(_3f,_40,_41){
for(var i=0;i<_41.length;i+=6){
_3f.push("bezierCurveTo",[this.last.x+_41[i],this.last.y+_41[i+1],this.lastControl.x=this.last.x+_41[i+2],this.lastControl.y=this.last.y+_41[i+3],this.last.x+_41[i+4],this.last.y+_41[i+5]]);
this.last.x+=_41[i+4];
this.last.y+=_41[i+5];
}
this.lastControl.type="C";
},_smoothCurveToA:function(_42,_43,_44){
for(var i=0;i<_44.length;i+=4){
var _45=this.lastControl.type=="C";
_42.push("bezierCurveTo",[_45?2*this.last.x-this.lastControl.x:this.last.x,_45?2*this.last.y-this.lastControl.y:this.last.y,_44[i],_44[i+1],_44[i+2],_44[i+3]]);
this.lastControl.x=_44[i];
this.lastControl.y=_44[i+1];
this.lastControl.type="C";
}
this.last.x=_44[_44.length-2];
this.last.y=_44[_44.length-1];
},_smoothCurveToR:function(_46,_47,_48){
for(var i=0;i<_48.length;i+=4){
var _49=this.lastControl.type=="C";
_46.push("bezierCurveTo",[_49?2*this.last.x-this.lastControl.x:this.last.x,_49?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_48[i],this.last.y+_48[i+1],this.last.x+_48[i+2],this.last.y+_48[i+3]]);
this.lastControl.x=this.last.x+_48[i];
this.lastControl.y=this.last.y+_48[i+1];
this.lastControl.type="C";
this.last.x+=_48[i+2];
this.last.y+=_48[i+3];
}
},_qCurveToA:function(_4a,_4b,_4c){
for(var i=0;i<_4c.length;i+=4){
_4a.push("quadraticCurveTo",_4c.slice(i,i+4));
}
this.last.x=_4c[_4c.length-2];
this.last.y=_4c[_4c.length-1];
this.lastControl.x=_4c[_4c.length-4];
this.lastControl.y=_4c[_4c.length-3];
this.lastControl.type="Q";
},_qCurveToR:function(_4d,_4e,_4f){
for(var i=0;i<_4f.length;i+=4){
_4d.push("quadraticCurveTo",[this.lastControl.x=this.last.x+_4f[i],this.lastControl.y=this.last.y+_4f[i+1],this.last.x+_4f[i+2],this.last.y+_4f[i+3]]);
this.last.x+=_4f[i+2];
this.last.y+=_4f[i+3];
}
this.lastControl.type="Q";
},_qSmoothCurveToA:function(_50,_51,_52){
for(var i=0;i<_52.length;i+=2){
var _53=this.lastControl.type=="Q";
_50.push("quadraticCurveTo",[this.lastControl.x=_53?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_53?2*this.last.y-this.lastControl.y:this.last.y,_52[i],_52[i+1]]);
this.lastControl.type="Q";
}
this.last.x=_52[_52.length-2];
this.last.y=_52[_52.length-1];
},_qSmoothCurveToR:function(_54,_55,_56){
for(var i=0;i<_56.length;i+=2){
var _57=this.lastControl.type=="Q";
_54.push("quadraticCurveTo",[this.lastControl.x=_57?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_57?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_56[i],this.last.y+_56[i+1]]);
this.lastControl.type="Q";
this.last.x+=_56[i];
this.last.y+=_56[i+1];
}
},_arcTo:function(_58,_59,_5a){
var _5b=_59=="a";
for(var i=0;i<_5a.length;i+=7){
var x1=_5a[i+5],y1=_5a[i+6];
if(_5b){
x1+=this.last.x;
y1+=this.last.y;
}
var _5c=ga.arcAsBezier(this.last,_5a[i],_5a[i+1],_5a[i+2],_5a[i+3]?1:0,_5a[i+4]?1:0,x1,y1);
_2.forEach(_5c,function(p){
_58.push("bezierCurveTo",p);
});
this.last.x=x1;
this.last.y=y1;
}
this.lastControl={};
},_closePath:function(_5d,_5e,_5f){
_5d.push("closePath",[]);
this.lastControl={};
}});
_2.forEach(["moveTo","lineTo","hLineTo","vLineTo","curveTo","smoothCurveTo","qCurveTo","qSmoothCurveTo","arcTo","closePath"],function(_60){
_1a(_a.Path,_60);
});
_a.TextPath=_3("dojox.gfx.canvas.TextPath",[_a.Shape,_8.TextPath],{_renderShape:function(ctx){
var s=this.shape;
},_setText:function(){
},_setFont:function(){
}});
_a.Surface=_3("dojox.gfx.canvas.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
this.pendingImageCount=0;
this.makeDirty();
},destroy:function(){
gs.Container.clear.call(this,true);
this.inherited(arguments);
},setDimensions:function(_61,_62){
this.width=g.normalizedLength(_61);
this.height=g.normalizedLength(_62);
if(!this.rawNode){
return this;
}
var _63=false;
if(this.rawNode.width!=this.width){
this.rawNode.width=this.width;
_63=true;
}
if(this.rawNode.height!=this.height){
this.rawNode.height=this.height;
_63=true;
}
if(_63){
this.makeDirty();
}
return this;
},getDimensions:function(){
return this.rawNode?{width:this.rawNode.width,height:this.rawNode.height}:null;
},_render:function(_64){
if(!this.rawNode||(!_64&&this.pendingImageCount)){
return;
}
var ctx=this.rawNode.getContext("2d");
ctx.clearRect(0,0,this.rawNode.width,this.rawNode.height);
this.render(ctx);
if("pendingRender" in this){
clearTimeout(this.pendingRender);
delete this.pendingRender;
}
},render:function(ctx){
ctx.save();
for(var i=0;i<this.children.length;++i){
this.children[i]._render(ctx);
}
ctx.restore();
},makeDirty:function(){
if(!this.pendingImagesCount&&!("pendingRender" in this)){
this.pendingRender=setTimeout(_1.hitch(this,this._render),0);
}
},downloadImage:function(img,url){
var _65=_1.hitch(this,this.onImageLoad);
if(!this.pendingImageCount++&&"pendingRender" in this){
clearTimeout(this.pendingRender);
delete this.pendingRender;
}
img.onload=_65;
img.onerror=_65;
img.onabort=_65;
img.src=url;
},onImageLoad:function(){
if(!--this.pendingImageCount){
this.onImagesLoaded();
this._render();
}
},onImagesLoaded:function(){
},getEventSource:function(){
return null;
},connect:function(){
},disconnect:function(){
}});
_a.createSurface=function(_66,_67,_68){
if(!_67&&!_68){
var pos=_5.position(_66);
_67=_67||pos.w;
_68=_68||pos.h;
}
if(typeof _67=="number"){
_67=_67+"px";
}
if(typeof _68=="number"){
_68=_68+"px";
}
var s=new _a.Surface(),p=_6.byId(_66),c=p.ownerDocument.createElement("canvas");
c.width=g.normalizedLength(_67);
c.height=g.normalizedLength(_68);
p.appendChild(c);
s.rawNode=c;
s._parent=p;
s.surface=s;
return s;
};
var C=gs.Container,_69={add:function(_6a){
this.surface.makeDirty();
return C.add.apply(this,arguments);
},remove:function(_6b,_6c){
this.surface.makeDirty();
return C.remove.apply(this,arguments);
},clear:function(){
this.surface.makeDirty();
return C.clear.apply(this,arguments);
},getBoundingBox:C.getBoundingBox,_moveChildToFront:function(_6d){
this.surface.makeDirty();
return C._moveChildToFront.apply(this,arguments);
},_moveChildToBack:function(_6e){
this.surface.makeDirty();
return C._moveChildToBack.apply(this,arguments);
}};
var _6f={createObject:function(_70,_71){
var _72=new _70();
_72.surface=this.surface;
_72.setShape(_71);
this.add(_72);
return _72;
}};
_e(_a.Group,_69);
_e(_a.Group,gs.Creator);
_e(_a.Group,_6f);
_e(_a.Surface,_69);
_e(_a.Surface,gs.Creator);
_e(_a.Surface,_6f);
_a.fixTarget=function(_73,_74){
return true;
};
return _a;
});
