//>>built
define("dojox/gfx/canvas",["./_base","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-geometry","dojo/dom","./shape","./path","./arc","./matrix","./decompose","./bezierutils"],function(g,_1,_2,_3,_4,_5,_6,gs,_7,ga,m,_8,_9){
var _a=g.canvas={};
var _b=null,mp=m.multiplyPoint,pi=Math.PI,_c=2*pi,_d=pi/2,_e=_1.extend;
if(_4.global.CanvasRenderingContext2D){
var _f=_4.doc.createElement("canvas").getContext("2d");
var _10=typeof _f.setLineDash=="function";
var _11=typeof _f.fillText=="function";
}
var _12={solid:"none",shortdash:[4,1],shortdot:[1,1],shortdashdot:[4,1,1,1],shortdashdotdot:[4,1,1,1,1,1],dot:[1,3],dash:[4,3],longdash:[8,3],dashdot:[4,3,1,3],longdashdot:[8,3,1,3],longdashdotdot:[8,3,1,3,1,3]};
function _13(ctx,_14,cx,cy,r,sa,ea,ccw,_15,_16){
var _17,_18,l=_14.length,i=0;
if(_16){
_18=_16.l/r;
i=_16.i;
}else{
_18=_14[0]/r;
}
while(sa<ea){
if(sa+_18>ea){
_17={l:(sa+_18-ea)*r,i:i};
_18=ea-sa;
}
if(!(i%2)){
ctx.beginPath();
ctx.arc(cx,cy,r,sa,sa+_18,ccw);
if(_15){
ctx.stroke();
}
}
sa+=_18;
++i;
_18=_14[i%l]/r;
}
return _17;
};
function _19(_1a,_1b,_1c,_1d){
var _1e=0,t=0,_1f,i=0;
if(_1d){
_1f=_1d.l;
i=_1d.i;
}else{
_1f=_1b[0];
}
while(t<1){
t=_9.tAtLength(_1a,_1f);
if(t==1){
var rl=_9.computeLength(_1a);
_1e={l:_1f-rl,i:i};
}
var _20=_9.splitBezierAtT(_1a,t);
if(!(i%2)){
_1c.push(_20[0]);
}
_1a=_20[1];
++i;
_1f=_1b[i%_1b.length];
}
return _1e;
};
function _21(ctx,_22,_23,_24){
var pts=[_22.last.x,_22.last.y].concat(_23),_25=_23.length===4,_f=!(ctx instanceof Array),api=_25?"quadraticCurveTo":"bezierCurveTo",_26=[];
var _27=_19(pts,_22.canvasDash,_26,_24);
for(var c=0;c<_26.length;++c){
var _28=_26[c];
if(_f){
ctx.moveTo(_28[0],_28[1]);
ctx[api].apply(ctx,_28.slice(2));
}else{
ctx.push("moveTo",[_28[0],_28[1]]);
ctx.push(api,_28.slice(2));
}
}
return _27;
};
function _29(ctx,_2a,x1,y1,x2,y2,_2b){
var _2c=0,r=0,dal=0,_2d=_9.distance(x1,y1,x2,y2),i=0,_2e=_2a.canvasDash,_2f=x1,_30=y1,x,y,_f=!(ctx instanceof Array);
if(_2b){
dal=_2b.l;
i=_2b.i;
}else{
dal+=_2e[0];
}
while(Math.abs(1-r)>0.01){
if(dal>_2d){
_2c={l:dal-_2d,i:i};
dal=_2d;
}
r=dal/_2d;
x=x1+(x2-x1)*r;
y=y1+(y2-y1)*r;
if(!(i++%2)){
if(_f){
ctx.moveTo(_2f,_30);
ctx.lineTo(x,y);
}else{
ctx.push("moveTo",[_2f,_30]);
ctx.push("lineTo",[x,y]);
}
}
_2f=x;
_30=y;
dal+=_2e[i%_2e.length];
}
return _2c;
};
_a.Shape=_3("dojox.gfx.canvas.Shape",gs.Shape,{_render:function(ctx){
ctx.save();
this._renderTransform(ctx);
this._renderClip(ctx);
this._renderShape(ctx);
this._renderFill(ctx,true);
this._renderStroke(ctx,true);
ctx.restore();
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
},_renderFill:function(ctx,_31){
if("canvasFill" in this){
var fs=this.fillStyle;
if("canvasFillImage" in this){
var w=fs.width,h=fs.height,iw=this.canvasFillImage.width,ih=this.canvasFillImage.height,sx=w==iw?1:w/iw,sy=h==ih?1:h/ih,s=Math.min(sx,sy),dx=(w-s*iw)/2,dy=(h-s*ih)/2;
_b.width=w;
_b.height=h;
var _32=_b.getContext("2d");
_32.clearRect(0,0,w,h);
_32.drawImage(this.canvasFillImage,0,0,iw,ih,dx,dy,s*iw,s*ih);
this.canvasFill=ctx.createPattern(_b,"repeat");
delete this.canvasFillImage;
}
ctx.fillStyle=this.canvasFill;
if(_31){
if(fs.type==="pattern"&&(fs.x!==0||fs.y!==0)){
ctx.translate(fs.x,fs.y);
}
ctx.fill();
}
}else{
ctx.fillStyle="rgba(0,0,0,0.0)";
}
},_renderStroke:function(ctx,_33){
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
if(this.canvasDash){
if(_10){
ctx.setLineDash(this.canvasDash);
if(_33){
ctx.stroke();
}
}else{
this._renderDashedStroke(ctx,_33);
}
}else{
if(_33){
ctx.stroke();
}
}
}else{
if(!_33){
ctx.strokeStyle="rgba(0,0,0,0.0)";
}
}
},_renderDashedStroke:function(ctx,_34){
},getEventSource:function(){
return null;
},on:function(){
},connect:function(){
},disconnect:function(){
},canvasClip:null,setClip:function(_35){
this.inherited(arguments);
var _36=_35?"width" in _35?"rect":"cx" in _35?"ellipse":"points" in _35?"polyline":"d" in _35?"path":null:null;
if(_35&&!_36){
return this;
}
this.canvasClip=_35?_37(_36,_35):null;
if(this.parent){
this.parent._makeDirty();
}
return this;
}});
var _37=function(_38,_39){
switch(_38){
case "ellipse":
return {canvasEllipse:_3a({shape:_39}),render:function(ctx){
return _a.Ellipse.prototype._renderShape.call(this,ctx);
}};
case "rect":
return {shape:_1.delegate(_39,{r:0}),render:function(ctx){
return _a.Rect.prototype._renderShape.call(this,ctx);
}};
case "path":
return {canvasPath:_3b(_39),render:function(ctx){
this.canvasPath._renderShape(ctx);
}};
case "polyline":
return {canvasPolyline:_39.points,render:function(ctx){
return _a.Polyline.prototype._renderShape.call(this,ctx);
}};
}
return null;
};
var _3b=function(geo){
var p=new dojox.gfx.canvas.Path();
p.canvasPath=[];
p._setPath(geo.d);
return p;
};
var _3c=function(_3d,_3e,_3f){
var old=_3d.prototype[_3e];
_3d.prototype[_3e]=_3f?function(){
if(this.parent){
this.parent._makeDirty();
}
old.apply(this,arguments);
_3f.call(this);
return this;
}:function(){
if(this.parent){
this.parent._makeDirty();
}
return old.apply(this,arguments);
};
};
_3c(_a.Shape,"setTransform",function(){
if(this.matrix){
this.canvasTransform=g.decompose(this.matrix);
}else{
delete this.canvasTransform;
}
});
_3c(_a.Shape,"setFill",function(){
var fs=this.fillStyle,f;
if(fs){
if(typeof (fs)=="object"&&"type" in fs){
var ctx=this.surface.rawNode.getContext("2d");
switch(fs.type){
case "linear":
case "radial":
f=fs.type=="linear"?ctx.createLinearGradient(fs.x1,fs.y1,fs.x2,fs.y2):ctx.createRadialGradient(fs.cx,fs.cy,0,fs.cx,fs.cy,fs.r);
_2.forEach(fs.colors,function(_40){
f.addColorStop(_40.offset,g.normalizeColor(_40.color).toString());
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
_3c(_a.Shape,"setStroke",function(){
var st=this.strokeStyle;
if(st){
var da=this.strokeStyle.style.toLowerCase();
if(da in _12){
da=_12[da];
}
if(da instanceof Array){
da=da.slice();
this.canvasDash=da;
var i;
for(i=0;i<da.length;++i){
da[i]*=st.width;
}
if(st.cap!="butt"){
for(i=0;i<da.length;i+=2){
da[i]-=st.width;
if(da[i]<1){
da[i]=1;
}
}
for(i=1;i<da.length;i+=2){
da[i]+=st.width;
}
}
}else{
delete this.canvasDash;
}
}else{
delete this.canvasDash;
}
this._needsDash=!_10&&!!this.canvasDash;
});
_3c(_a.Shape,"setShape");
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
gs.Container.clear.call(this,true);
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
},_renderDashedStroke:function(ctx,_41){
var s=this.shape,_42,r=Math.min(s.r,s.height/2,s.width/2),xl=s.x,xr=xl+s.width,yt=s.y,yb=yt+s.height,xl2=xl+r,xr2=xr-r,yt2=yt+r,yb2=yb-r;
if(r){
ctx.beginPath();
_42=_29(ctx,this,xl2,yt,xr2,yt);
if(_41){
ctx.stroke();
}
_42=_13(ctx,this.canvasDash,xr2,yt2,r,-_d,0,false,_41,_42);
ctx.beginPath();
_42=_29(ctx,this,xr,yt2,xr,yb2,_42);
if(_41){
ctx.stroke();
}
_42=_13(ctx,this.canvasDash,xr2,yb2,r,0,_d,false,_41,_42);
ctx.beginPath();
_42=_29(ctx,this,xr2,yb,xl2,yb,_42);
if(_41){
ctx.stroke();
}
_42=_13(ctx,this.canvasDash,xl2,yb2,r,_d,pi,false,_41,_42);
ctx.beginPath();
_42=_29(ctx,this,xl,yb2,xl,yt2,_42);
if(_41){
ctx.stroke();
}
_13(ctx,this.canvasDash,xl2,yt2,r,pi,pi+_d,false,_41,_42);
}else{
ctx.beginPath();
_42=_29(ctx,this,xl2,yt,xr2,yt);
_42=_29(ctx,this,xr2,yt,xr,yb2,_42);
_42=_29(ctx,this,xr,yb2,xl2,yb,_42);
_29(ctx,this,xl2,yb,xl,yt2,_42);
if(_41){
ctx.stroke();
}
}
}});
var _43=[];
(function(){
var u=ga.curvePI4;
_43.push(u.s,u.c1,u.c2,u.e);
for(var a=45;a<360;a+=45){
var r=m.rotateg(a);
_43.push(mp(r,u.c1),mp(r,u.c2),mp(r,u.e));
}
})();
var _3a=function(_44){
var t,c1,c2,r=[],s=_44.shape,M=m.normalize([m.translate(s.cx,s.cy),m.scale(s.rx,s.ry)]);
t=mp(M,_43[0]);
r.push([t.x,t.y]);
for(var i=1;i<_43.length;i+=3){
c1=mp(M,_43[i]);
c2=mp(M,_43[i+1]);
t=mp(M,_43[i+2]);
r.push([c1.x,c1.y,c2.x,c2.y,t.x,t.y]);
}
if(_44._needsDash){
var _45=[],p1=r[0];
for(i=1;i<r.length;++i){
var _46=[];
_19(p1.concat(r[i]),_44.canvasDash,_46);
p1=[r[i][4],r[i][5]];
_45.push(_46);
}
_44._dashedPoints=_45;
}
return r;
};
_a.Ellipse=_3("dojox.gfx.canvas.Ellipse",[_a.Shape,gs.Ellipse],{setShape:function(){
this.inherited(arguments);
this.canvasEllipse=_3a(this);
return this;
},setStroke:function(){
this.inherited(arguments);
if(!_10){
this.canvasEllipse=_3a(this);
}
return this;
},_renderShape:function(ctx){
var r=this.canvasEllipse,i;
ctx.beginPath();
ctx.moveTo.apply(ctx,r[0]);
for(i=1;i<r.length;++i){
ctx.bezierCurveTo.apply(ctx,r[i]);
}
ctx.closePath();
},_renderDashedStroke:function(ctx,_47){
var r=this._dashedPoints;
ctx.beginPath();
for(var i=0;i<r.length;++i){
var _48=r[i];
for(var j=0;j<_48.length;++j){
var _49=_48[j];
ctx.moveTo(_49[0],_49[1]);
ctx.bezierCurveTo(_49[2],_49[3],_49[4],_49[5],_49[6],_49[7]);
}
}
if(_47){
ctx.stroke();
}
}});
_a.Circle=_3("dojox.gfx.canvas.Circle",[_a.Shape,gs.Circle],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.arc(s.cx,s.cy,s.r,0,_c,1);
},_renderDashedStroke:function(ctx,_4a){
var s=this.shape;
var _4b=0,_4c,l=this.canvasDash.length;
i=0;
while(_4b<_c){
_4c=this.canvasDash[i%l]/s.r;
if(!(i%2)){
ctx.beginPath();
ctx.arc(s.cx,s.cy,s.r,_4b,_4b+_4c,0);
if(_4a){
ctx.stroke();
}
}
_4b+=_4c;
++i;
}
}});
_a.Line=_3("dojox.gfx.canvas.Line",[_a.Shape,gs.Line],{_renderShape:function(ctx){
var s=this.shape;
ctx.beginPath();
ctx.moveTo(s.x1,s.y1);
ctx.lineTo(s.x2,s.y2);
},_renderDashedStroke:function(ctx,_4d){
var s=this.shape;
ctx.beginPath();
_29(ctx,this,s.x1,s.y1,s.x2,s.y2);
if(_4d){
ctx.stroke();
}
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
},_renderDashedStroke:function(ctx,_4e){
var p=this.canvasPolyline,_4f=0;
ctx.beginPath();
for(var i=0;i<p.length;i+=2){
_4f=_29(ctx,this,p[i],p[i+1],p[i+2],p[i+3],_4f);
}
if(_4e){
ctx.stroke();
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
if(s.text){
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
if(!s.text){
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
_3c(_a.Text,"setFont");
if(!_11){
_a.Text.extend({getTextWidth:function(){
return 0;
},getBoundingBox:function(){
return null;
},_renderShape:function(){
}});
}
var _50={M:"_moveToA",m:"_moveToR",L:"_lineToA",l:"_lineToR",H:"_hLineToA",h:"_hLineToR",V:"_vLineToA",v:"_vLineToR",C:"_curveToA",c:"_curveToR",S:"_smoothCurveToA",s:"_smoothCurveToR",Q:"_qCurveToA",q:"_qCurveToR",T:"_qSmoothCurveToA",t:"_qSmoothCurveToR",A:"_arcTo",a:"_arcTo",Z:"_closePath",z:"_closePath"};
_a.Path=_3("dojox.gfx.canvas.Path",[_a.Shape,_7.Path],{constructor:function(){
this.lastControl={};
},setShape:function(){
this.canvasPath=[];
this._dashedPath=[];
return this.inherited(arguments);
},setStroke:function(){
this.inherited(arguments);
if(!_10){
this.segmented=false;
this._confirmSegmented();
}
return this;
},_setPath:function(){
this._dashResidue=null;
this.inherited(arguments);
},_updateWithSegment:function(_51){
var _52=_1.clone(this.last);
this[_50[_51.action]](this.canvasPath,_51.action,_51.args,this._needsDash?this._dashedPath:null);
this.last=_52;
this.inherited(arguments);
},_renderShape:function(ctx){
var r=this.canvasPath;
ctx.beginPath();
for(var i=0;i<r.length;i+=2){
ctx[r[i]].apply(ctx,r[i+1]);
}
},_renderDashedStroke:_10?function(){
}:function(ctx,_53){
var r=this._dashedPath;
ctx.beginPath();
for(var i=0;i<r.length;i+=2){
ctx[r[i]].apply(ctx,r[i+1]);
}
if(_53){
ctx.stroke();
}
},_moveToA:function(_54,_55,_56,_57){
_54.push("moveTo",[_56[0],_56[1]]);
if(_57){
_57.push("moveTo",[_56[0],_56[1]]);
}
for(var i=2;i<_56.length;i+=2){
_54.push("lineTo",[_56[i],_56[i+1]]);
if(_57){
this._dashResidue=_29(_57,this,_56[i-2],_56[i-1],_56[i],_56[i+1],this._dashResidue);
}
}
this.last.x=_56[_56.length-2];
this.last.y=_56[_56.length-1];
this.lastControl={};
},_moveToR:function(_58,_59,_5a,_5b){
var pts;
if("x" in this.last){
pts=[this.last.x+=_5a[0],this.last.y+=_5a[1]];
_58.push("moveTo",pts);
if(_5b){
_5b.push("moveTo",pts);
}
}else{
pts=[this.last.x=_5a[0],this.last.y=_5a[1]];
_58.push("moveTo",pts);
if(_5b){
_5b.push("moveTo",pts);
}
}
for(var i=2;i<_5a.length;i+=2){
_58.push("lineTo",[this.last.x+=_5a[i],this.last.y+=_5a[i+1]]);
if(_5b){
this._dashResidue=_29(_5b,this,_5b[_5b.length-1][0],_5b[_5b.length-1][1],this.last.x,this.last.y,this._dashResidue);
}
}
this.lastControl={};
},_lineToA:function(_5c,_5d,_5e,_5f){
for(var i=0;i<_5e.length;i+=2){
if(_5f){
this._dashResidue=_29(_5f,this,this.last.x,this.last.y,_5e[i],_5e[i+1],this._dashResidue);
}
_5c.push("lineTo",[_5e[i],_5e[i+1]]);
}
this.last.x=_5e[_5e.length-2];
this.last.y=_5e[_5e.length-1];
this.lastControl={};
},_lineToR:function(_60,_61,_62,_63){
for(var i=0;i<_62.length;i+=2){
_60.push("lineTo",[this.last.x+=_62[i],this.last.y+=_62[i+1]]);
if(_63){
this._dashResidue=_29(_63,this,_63[_63.length-1][0],_63[_63.length-1][1],this.last.x,this.last.y,this._dashResidue);
}
}
this.lastControl={};
},_hLineToA:function(_64,_65,_66,_67){
for(var i=0;i<_66.length;++i){
_64.push("lineTo",[_66[i],this.last.y]);
if(_67){
this._dashResidue=_29(_67,this,_67[_67.length-1][0],_67[_67.length-1][1],_66[i],this.last.y,this._dashResidue);
}
}
this.last.x=_66[_66.length-1];
this.lastControl={};
},_hLineToR:function(_68,_69,_6a,_6b){
for(var i=0;i<_6a.length;++i){
_68.push("lineTo",[this.last.x+=_6a[i],this.last.y]);
if(_6b){
this._dashResidue=_29(_6b,this,_6b[_6b.length-1][0],_6b[_6b.length-1][1],this.last.x,this.last.y,this._dashResidue);
}
}
this.lastControl={};
},_vLineToA:function(_6c,_6d,_6e,_6f){
for(var i=0;i<_6e.length;++i){
_6c.push("lineTo",[this.last.x,_6e[i]]);
if(_6f){
this._dashResidue=_29(_6f,this,_6f[_6f.length-1][0],_6f[_6f.length-1][1],this.last.x,_6e[i],this._dashResidue);
}
}
this.last.y=_6e[_6e.length-1];
this.lastControl={};
},_vLineToR:function(_70,_71,_72,_73){
for(var i=0;i<_72.length;++i){
_70.push("lineTo",[this.last.x,this.last.y+=_72[i]]);
if(_73){
this._dashResidue=_29(_73,this,_73[_73.length-1][0],_73[_73.length-1][1],this.last.x,this.last.y,this._dashResidue);
}
}
this.lastControl={};
},_curveToA:function(_74,_75,_76,_77){
for(var i=0;i<_76.length;i+=6){
_74.push("bezierCurveTo",_76.slice(i,i+6));
if(_77){
this._dashResidue=_21(_77,this,_74[_74.length-1],this._dashResidue);
}
}
this.last.x=_76[_76.length-2];
this.last.y=_76[_76.length-1];
this.lastControl.x=_76[_76.length-4];
this.lastControl.y=_76[_76.length-3];
this.lastControl.type="C";
},_curveToR:function(_78,_79,_7a,_7b){
for(var i=0;i<_7a.length;i+=6){
_78.push("bezierCurveTo",[this.last.x+_7a[i],this.last.y+_7a[i+1],this.lastControl.x=this.last.x+_7a[i+2],this.lastControl.y=this.last.y+_7a[i+3],this.last.x+_7a[i+4],this.last.y+_7a[i+5]]);
if(_7b){
this._dashResidue=_21(_7b,this,_78[_78.length-1],this._dashResidue);
}
this.last.x+=_7a[i+4];
this.last.y+=_7a[i+5];
}
this.lastControl.type="C";
},_smoothCurveToA:function(_7c,_7d,_7e,_7f){
for(var i=0;i<_7e.length;i+=4){
var _80=this.lastControl.type=="C";
_7c.push("bezierCurveTo",[_80?2*this.last.x-this.lastControl.x:this.last.x,_80?2*this.last.y-this.lastControl.y:this.last.y,_7e[i],_7e[i+1],_7e[i+2],_7e[i+3]]);
if(_7f){
this._dashResidue=_21(_7f,this,_7c[_7c.length-1],this._dashResidue);
}
this.lastControl.x=_7e[i];
this.lastControl.y=_7e[i+1];
this.lastControl.type="C";
}
this.last.x=_7e[_7e.length-2];
this.last.y=_7e[_7e.length-1];
},_smoothCurveToR:function(_81,_82,_83,_84){
for(var i=0;i<_83.length;i+=4){
var _85=this.lastControl.type=="C";
_81.push("bezierCurveTo",[_85?2*this.last.x-this.lastControl.x:this.last.x,_85?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_83[i],this.last.y+_83[i+1],this.last.x+_83[i+2],this.last.y+_83[i+3]]);
if(_84){
this._dashResidue=_21(_84,this,_81[_81.length-1],this._dashResidue);
}
this.lastControl.x=this.last.x+_83[i];
this.lastControl.y=this.last.y+_83[i+1];
this.lastControl.type="C";
this.last.x+=_83[i+2];
this.last.y+=_83[i+3];
}
},_qCurveToA:function(_86,_87,_88,_89){
for(var i=0;i<_88.length;i+=4){
_86.push("quadraticCurveTo",_88.slice(i,i+4));
}
if(_89){
this._dashResidue=_21(_89,this,_86[_86.length-1],this._dashResidue);
}
this.last.x=_88[_88.length-2];
this.last.y=_88[_88.length-1];
this.lastControl.x=_88[_88.length-4];
this.lastControl.y=_88[_88.length-3];
this.lastControl.type="Q";
},_qCurveToR:function(_8a,_8b,_8c,_8d){
for(var i=0;i<_8c.length;i+=4){
_8a.push("quadraticCurveTo",[this.lastControl.x=this.last.x+_8c[i],this.lastControl.y=this.last.y+_8c[i+1],this.last.x+_8c[i+2],this.last.y+_8c[i+3]]);
if(_8d){
this._dashResidue=_21(_8d,this,_8a[_8a.length-1],this._dashResidue);
}
this.last.x+=_8c[i+2];
this.last.y+=_8c[i+3];
}
this.lastControl.type="Q";
},_qSmoothCurveToA:function(_8e,_8f,_90,_91){
for(var i=0;i<_90.length;i+=2){
var _92=this.lastControl.type=="Q";
_8e.push("quadraticCurveTo",[this.lastControl.x=_92?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_92?2*this.last.y-this.lastControl.y:this.last.y,_90[i],_90[i+1]]);
if(_91){
this._dashResidue=_21(_91,this,_8e[_8e.length-1],this._dashResidue);
}
this.lastControl.type="Q";
}
this.last.x=_90[_90.length-2];
this.last.y=_90[_90.length-1];
},_qSmoothCurveToR:function(_93,_94,_95,_96){
for(var i=0;i<_95.length;i+=2){
var _97=this.lastControl.type=="Q";
_93.push("quadraticCurveTo",[this.lastControl.x=_97?2*this.last.x-this.lastControl.x:this.last.x,this.lastControl.y=_97?2*this.last.y-this.lastControl.y:this.last.y,this.last.x+_95[i],this.last.y+_95[i+1]]);
if(_96){
this._dashResidue=_21(_96,this,_93[_93.length-1],this._dashResidue);
}
this.lastControl.type="Q";
this.last.x+=_95[i];
this.last.y+=_95[i+1];
}
},_arcTo:function(_98,_99,_9a,_9b){
var _9c=_99=="a";
for(var i=0;i<_9a.length;i+=7){
var x1=_9a[i+5],y1=_9a[i+6];
if(_9c){
x1+=this.last.x;
y1+=this.last.y;
}
var _9d=ga.arcAsBezier(this.last,_9a[i],_9a[i+1],_9a[i+2],_9a[i+3]?1:0,_9a[i+4]?1:0,x1,y1);
_2.forEach(_9d,function(p){
_98.push("bezierCurveTo",p);
});
if(_9b){
this._dashResidue=_21(_9b,this,p,this._dashResidue);
}
this.last.x=x1;
this.last.y=y1;
}
this.lastControl={};
},_closePath:function(_9e,_9f,_a0,_a1){
_9e.push("closePath",[]);
if(_a1){
this._dashResidue=_29(_a1,this,this.last.x,this.last.y,_a1[1][0],_a1[1][1],this._dashResidue);
}
this.lastControl={};
}});
_2.forEach(["moveTo","lineTo","hLineTo","vLineTo","curveTo","smoothCurveTo","qCurveTo","qSmoothCurveTo","arcTo","closePath"],function(_a2){
_3c(_a.Path,_a2);
});
_a.TextPath=_3("dojox.gfx.canvas.TextPath",[_a.Shape,_7.TextPath],{_renderShape:function(ctx){
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
},setDimensions:function(_a3,_a4){
this.width=g.normalizedLength(_a3);
this.height=g.normalizedLength(_a4);
if(!this.rawNode){
return this;
}
var _a5=false;
if(this.rawNode.width!=this.width){
this.rawNode.width=this.width;
_a5=true;
}
if(this.rawNode.height!=this.height){
this.rawNode.height=this.height;
_a5=true;
}
if(_a5){
this.makeDirty();
}
return this;
},getDimensions:function(){
return this.rawNode?{width:this.rawNode.width,height:this.rawNode.height}:null;
},_render:function(_a6){
if(!this.rawNode||(!_a6&&this.pendingImageCount)){
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
if(!this.pendingImagesCount&&!("pendingRender" in this)&&!this._batch){
this.pendingRender=setTimeout(_1.hitch(this,this._render),0);
}
},downloadImage:function(img,url){
var _a7=_1.hitch(this,this.onImageLoad);
if(!this.pendingImageCount++&&"pendingRender" in this){
clearTimeout(this.pendingRender);
delete this.pendingRender;
}
img.onload=_a7;
img.onerror=_a7;
img.onabort=_a7;
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
},on:function(){
}});
_a.createSurface=function(_a8,_a9,_aa){
if(!_a9&&!_aa){
var pos=_5.position(_a8);
_a9=_a9||pos.w;
_aa=_aa||pos.h;
}
if(typeof _a9=="number"){
_a9=_a9+"px";
}
if(typeof _aa=="number"){
_aa=_aa+"px";
}
var s=new _a.Surface(),p=_6.byId(_a8),c=p.ownerDocument.createElement("canvas");
c.width=g.normalizedLength(_a9);
c.height=g.normalizedLength(_aa);
p.appendChild(c);
s.rawNode=c;
s._parent=p;
s.surface=s;
return s;
};
var C=gs.Container,_ab={openBatch:function(){
++this._batch;
return this;
},closeBatch:function(){
this._batch=this._batch>0?--this._batch:0;
this._makeDirty();
return this;
},_makeDirty:function(){
if(!this._batch){
this.surface.makeDirty();
}
},add:function(_ac){
this._makeDirty();
return C.add.apply(this,arguments);
},remove:function(_ad,_ae){
this._makeDirty();
return C.remove.apply(this,arguments);
},clear:function(){
this._makeDirty();
return C.clear.apply(this,arguments);
},getBoundingBox:C.getBoundingBox,_moveChildToFront:function(_af){
this._makeDirty();
return C._moveChildToFront.apply(this,arguments);
},_moveChildToBack:function(_b0){
this._makeDirty();
return C._moveChildToBack.apply(this,arguments);
}};
var _b1={createObject:function(_b2,_b3){
var _b4=new _b2();
_b4.surface=this.surface;
_b4.setShape(_b3);
this.add(_b4);
return _b4;
}};
_e(_a.Group,_ab);
_e(_a.Group,gs.Creator);
_e(_a.Group,_b1);
_e(_a.Surface,_ab);
_e(_a.Surface,gs.Creator);
_e(_a.Surface,_b1);
_a.fixTarget=function(_b5,_b6){
return true;
};
return _a;
});
