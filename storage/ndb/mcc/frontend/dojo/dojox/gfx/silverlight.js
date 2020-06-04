//>>built
define("dojox/gfx/silverlight",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","dojo/on","dojo/_base/array","dojo/dom-geometry","dojo/dom","dojo/_base/sniff","./_base","./shape","./path","./registry"],function(_1,_2,_3,_4,on,_5,_6,_7,_8,g,gs,_9){
var sl=g.silverlight={};
_1.experimental("dojox.gfx.silverlight");
var _a={solid:"none",shortdash:[4,1],shortdot:[1,1],shortdashdot:[4,1,1,1],shortdashdotdot:[4,1,1,1,1,1],dot:[1,3],dash:[4,3],longdash:[8,3],dashdot:[4,3,1,3],longdashdot:[8,3,1,3],longdashdotdot:[8,3,1,3,1,3]},_b={normal:400,bold:700},_c={butt:"Flat",round:"Round",square:"Square"},_d={bevel:"Bevel",round:"Round"},_e={serif:"Times New Roman",times:"Times New Roman","sans-serif":"Arial",helvetica:"Arial",monotone:"Courier New",courier:"Courier New"};
function _f(_10){
var c=g.normalizeColor(_10),t=c.toHex(),a=Math.round(c.a*255);
a=(a<0?0:a>255?255:a).toString(16);
return "#"+(a.length<2?"0"+a:a)+t.slice(1);
};
sl.Shape=_3("dojox.gfx.silverlight.Shape",gs.Shape,{destroy:function(){
if(_8("gfxRegistry")){
gs.dispose(this);
}
this.rawNode=null;
},setFill:function(_11){
var p=this.rawNode.getHost().content,r=this.rawNode,f;
if(!_11){
this.fillStyle=null;
this._setFillAttr(null);
return this;
}
if(typeof (_11)=="object"&&"type" in _11){
switch(_11.type){
case "linear":
this.fillStyle=f=g.makeParameters(g.defaultLinearGradient,_11);
var lgb=p.createFromXaml("<LinearGradientBrush/>");
lgb.mappingMode="Absolute";
lgb.startPoint=f.x1+","+f.y1;
lgb.endPoint=f.x2+","+f.y2;
_5.forEach(f.colors,function(c){
var t=p.createFromXaml("<GradientStop/>");
t.offset=c.offset;
t.color=_f(c.color);
lgb.gradientStops.add(t);
});
this._setFillAttr(lgb);
break;
case "radial":
this.fillStyle=f=g.makeParameters(g.defaultRadialGradient,_11);
var rgb=p.createFromXaml("<RadialGradientBrush/>"),c=g.matrix.multiplyPoint(g.matrix.invert(this._getAdjustedMatrix()),f.cx,f.cy),pt=c.x+","+c.y;
rgb.mappingMode="Absolute";
rgb.gradientOrigin=pt;
rgb.center=pt;
rgb.radiusX=rgb.radiusY=f.r;
_5.forEach(f.colors,function(c){
var t=p.createFromXaml("<GradientStop/>");
t.offset=c.offset;
t.color=_f(c.color);
rgb.gradientStops.add(t);
});
this._setFillAttr(rgb);
break;
case "pattern":
this.fillStyle=null;
this._setFillAttr(null);
break;
}
return this;
}
this.fillStyle=f=g.normalizeColor(_11);
var scb=p.createFromXaml("<SolidColorBrush/>");
scb.color=f.toHex();
scb.opacity=f.a;
this._setFillAttr(scb);
return this;
},_setFillAttr:function(f){
this.rawNode.fill=f;
},setStroke:function(_12){
var p=this.rawNode.getHost().content,r=this.rawNode;
if(!_12){
this.strokeStyle=null;
r.stroke=null;
return this;
}
if(typeof _12=="string"||_2.isArray(_12)||_12 instanceof _4){
_12={color:_12};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_12);
s.color=g.normalizeColor(s.color);
if(s){
var scb=p.createFromXaml("<SolidColorBrush/>");
scb.color=s.color.toHex();
scb.opacity=s.color.a;
r.stroke=scb;
r.strokeThickness=s.width;
r.strokeStartLineCap=r.strokeEndLineCap=r.strokeDashCap=_c[s.cap];
if(typeof s.join=="number"){
r.strokeLineJoin="Miter";
r.strokeMiterLimit=s.join;
}else{
r.strokeLineJoin=_d[s.join];
}
var da=s.style.toLowerCase();
if(da in _a){
da=_a[da];
}
if(da instanceof Array){
da=_2.clone(da);
var i;
if(s.cap!="butt"){
for(i=0;i<da.length;i+=2){
--da[i];
if(da[i]<1){
da[i]=1;
}
}
for(i=1;i<da.length;i+=2){
++da[i];
}
}
r.strokeDashArray=da.join(",");
}else{
r.strokeDashArray=null;
}
}
return this;
},_getParentSurface:function(){
var _13=this.parent;
for(;_13&&!(_13 instanceof g.Surface);_13=_13.parent){
}
return _13;
},_applyTransform:function(){
var tm=this._getAdjustedMatrix(),r=this.rawNode;
if(tm){
var p=this.rawNode.getHost().content,mt=p.createFromXaml("<MatrixTransform/>"),mm=p.createFromXaml("<Matrix/>");
mm.m11=tm.xx;
mm.m21=tm.xy;
mm.m12=tm.yx;
mm.m22=tm.yy;
mm.offsetX=tm.dx;
mm.offsetY=tm.dy;
mt.matrix=mm;
r.renderTransform=mt;
}else{
r.renderTransform=null;
}
return this;
},setRawNode:function(_14){
_14.fill=null;
_14.stroke=null;
this.rawNode=_14;
this.rawNode.tag=this.getUID();
},_moveToFront:function(){
var c=this.parent.rawNode.children,r=this.rawNode;
c.remove(r);
c.add(r);
return this;
},_moveToBack:function(){
var c=this.parent.rawNode.children,r=this.rawNode;
c.remove(r);
c.insert(0,r);
return this;
},_getAdjustedMatrix:function(){
return this.matrix;
},setClip:function(_15){
this.inherited(arguments);
var r=this.rawNode;
if(_15){
var _16=_15?"width" in _15?"rect":"cx" in _15?"ellipse":"points" in _15?"polyline":"d" in _15?"path":null:null;
if(_15&&!_16){
return this;
}
var _17=this.getBoundingBox()||{x:0,y:0,width:0,height:0};
var _18="1,0,0,1,"+(-_17.x)+","+(-_17.y);
switch(_16){
case "rect":
r.clip=r.getHost().content.createFromXaml("<RectangleGeometry/>");
r.clip.rect=_15.x+","+_15.y+","+_15.width+","+_15.height;
r.clip.transform=_18;
break;
case "ellipse":
r.clip=r.getHost().content.createFromXaml("<EllipseGeometry/>");
r.clip.center=_15.cx+","+_15.cy;
r.clip.radiusX=_15.rx;
r.clip.radiusY=_15.ry;
r.clip.transform="1,0,0,1,"+(-_17.x)+","+(-_17.y);
break;
case "polyline":
if(_15.points.length>2){
var _19,_1a=r.getHost().content.createFromXaml("<PathGeometry/>"),_1b=r.getHost().content.createFromXaml("<PathFigure/>");
_1b.StartPoint=_15.points[0]+","+_15.points[1];
for(var i=2;i<=_15.points.length-2;i=i+2){
_19=r.getHost().content.createFromXaml("<LineSegment/>");
_19.Point=_15.points[i]+","+_15.points[i+1];
_1b.segments.add(_19);
}
_1a.figures.add(_1b);
_1a.transform="1,0,0,1,"+(-_17.x)+","+(-_17.y);
r.clip=_1a;
}
break;
case "path":
break;
}
}else{
r.clip=null;
}
return this;
}});
sl.Group=_3("dojox.gfx.silverlight.Group",sl.Shape,{constructor:function(){
gs.Container._init.call(this);
},setRawNode:function(_1c){
this.rawNode=_1c;
this.rawNode.tag=this.getUID();
},destroy:function(){
this.clear(true);
sl.Shape.prototype.destroy.apply(this,arguments);
}});
sl.Group.nodeType="Canvas";
sl.Rect=_3("dojox.gfx.silverlight.Rect",[sl.Shape,gs.Rect],{setShape:function(_1d){
this.shape=g.makeParameters(this.shape,_1d);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=n.width;
r.height=n.height;
r.radiusX=r.radiusY=n.r;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _1e=this.matrix,s=this.shape,_1f={dx:s.x,dy:s.y};
return new g.Matrix2D(_1e?[_1e,_1f]:_1f);
}});
sl.Rect.nodeType="Rectangle";
sl.Ellipse=_3("dojox.gfx.silverlight.Ellipse",[sl.Shape,gs.Ellipse],{setShape:function(_20){
this.shape=g.makeParameters(this.shape,_20);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=2*n.rx;
r.height=2*n.ry;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _21=this.matrix,s=this.shape,_22={dx:s.cx-s.rx,dy:s.cy-s.ry};
return new g.Matrix2D(_21?[_21,_22]:_22);
}});
sl.Ellipse.nodeType="Ellipse";
sl.Circle=_3("dojox.gfx.silverlight.Circle",[sl.Shape,gs.Circle],{setShape:function(_23){
this.shape=g.makeParameters(this.shape,_23);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=r.height=2*n.r;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _24=this.matrix,s=this.shape,_25={dx:s.cx-s.r,dy:s.cy-s.r};
return new g.Matrix2D(_24?[_24,_25]:_25);
}});
sl.Circle.nodeType="Ellipse";
sl.Line=_3("dojox.gfx.silverlight.Line",[sl.Shape,gs.Line],{setShape:function(_26){
this.shape=g.makeParameters(this.shape,_26);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.x1=n.x1;
r.y1=n.y1;
r.x2=n.x2;
r.y2=n.y2;
return this;
}});
sl.Line.nodeType="Line";
sl.Polyline=_3("dojox.gfx.silverlight.Polyline",[sl.Shape,gs.Polyline],{setShape:function(_27,_28){
if(_27&&_27 instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_27});
if(_28&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_27);
}
this.bbox=null;
this._normalizePoints();
var p=this.shape.points,rp=[];
for(var i=0;i<p.length;++i){
rp.push(p[i].x,p[i].y);
}
this.rawNode.points=rp.join(",");
return this;
}});
sl.Polyline.nodeType="Polyline";
sl.Image=_3("dojox.gfx.silverlight.Image",[sl.Shape,gs.Image],{setShape:function(_29){
this.shape=g.makeParameters(this.shape,_29);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=n.width;
r.height=n.height;
r.source=n.src;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _2a=this.matrix,s=this.shape,_2b={dx:s.x,dy:s.y};
return new g.Matrix2D(_2a?[_2a,_2b]:_2b);
},setRawNode:function(_2c){
this.rawNode=_2c;
this.rawNode.tag=this.getUID();
}});
sl.Image.nodeType="Image";
sl.Text=_3("dojox.gfx.silverlight.Text",[sl.Shape,gs.Text],{setShape:function(_2d){
this.shape=g.makeParameters(this.shape,_2d);
this.bbox=null;
var r=this.rawNode,s=this.shape;
r.text=""+s.text;
r.textDecorations=s.decoration==="underline"?"Underline":"None";
r["Canvas.Left"]=-10000;
r["Canvas.Top"]=-10000;
if(!this._delay){
this._delay=window.setTimeout(_2.hitch(this,"_delayAlignment"),10);
}
return this;
},_delayAlignment:function(){
var r=this.rawNode,s=this.shape,w,h;
try{
w=r.actualWidth;
h=r.actualHeight;
}
catch(e){
return;
}
var x=s.x,y=s.y-h*0.75;
switch(s.align){
case "middle":
x-=w/2;
break;
case "end":
x-=w;
break;
}
this._delta={dx:x,dy:y};
r["Canvas.Left"]=0;
r["Canvas.Top"]=0;
this._applyTransform();
delete this._delay;
},_getAdjustedMatrix:function(){
var _2e=this.matrix,_2f=this._delta,x;
if(_2e){
x=_2f?[_2e,_2f]:_2e;
}else{
x=_2f?_2f:{};
}
return new g.Matrix2D(x);
},setStroke:function(){
return this;
},_setFillAttr:function(f){
this.rawNode.foreground=f;
},setRawNode:function(_30){
this.rawNode=_30;
this.rawNode.tag=this.getUID();
},getTextWidth:function(){
return this.rawNode.actualWidth;
},getBoundingBox:function(){
var _31=null,_32=this.getShape().text,r=this.rawNode,w=0,h=0;
if(!g._base._isRendered(this)){
return {x:0,y:0,width:0,height:0};
}
if(_32){
try{
w=r.actualWidth;
h=r.actualHeight;
}
catch(e){
return null;
}
var loc=g._base._computeTextLocation(this.getShape(),w,h,true);
_31={x:loc.x,y:loc.y,width:w,height:h};
}
return _31;
}});
sl.Text.nodeType="TextBlock";
sl.Path=_3("dojox.gfx.silverlight.Path",[sl.Shape,_9.Path],{_updateWithSegment:function(_33){
this.inherited(arguments);
var p=this.shape.path;
if(typeof (p)=="string"){
this.rawNode.data=p?p:null;
}
},setShape:function(_34){
this.inherited(arguments);
var p=this.shape.path;
this.rawNode.data=p?p:null;
return this;
}});
sl.Path.nodeType="Path";
sl.TextPath=_3("dojox.gfx.silverlight.TextPath",[sl.Shape,_9.TextPath],{_updateWithSegment:function(_35){
},setShape:function(_36){
},_setText:function(){
}});
sl.TextPath.nodeType="text";
var _37={},_38=function(){
};
sl.Surface=_3("dojox.gfx.silverlight.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
this.clear(true);
window[this._onLoadName]=_38;
delete _37[this._nodeName];
this.inherited(arguments);
},setDimensions:function(_39,_3a){
this.width=g.normalizedLength(_39);
this.height=g.normalizedLength(_3a);
var p=this.rawNode&&this.rawNode.getHost();
if(p){
p.width=_39;
p.height=_3a;
}
return this;
},getDimensions:function(){
var p=this.rawNode&&this.rawNode.getHost();
var t=p?{width:p.content.actualWidth,height:p.content.actualHeight}:null;
if(t.width<=0){
t.width=this.width;
}
if(t.height<=0){
t.height=this.height;
}
return t;
}});
sl.createSurface=function(_3b,_3c,_3d){
if(!_3c&&!_3d){
var pos=_6.position(_3b);
_3c=_3c||pos.w;
_3d=_3d||pos.h;
}
if(typeof _3c=="number"){
_3c=_3c+"px";
}
if(typeof _3d=="number"){
_3d=_3d+"px";
}
var s=new sl.Surface();
_3b=_7.byId(_3b);
s._parent=_3b;
s._nodeName=g._base._getUniqueId();
var t=_3b.ownerDocument.createElement("script");
t.type="text/xaml";
t.id=g._base._getUniqueId();
t.text="<?xml version='1.0'?><Canvas xmlns='http://schemas.microsoft.com/client/2007' Name='"+s._nodeName+"'/>";
_3b.parentNode.insertBefore(t,_3b);
s._nodes.push(t);
var obj,_3e=g._base._getUniqueId(),_3f="__"+g._base._getUniqueId()+"_onLoad";
s._onLoadName=_3f;
window[_3f]=function(_40){
if(!s.rawNode){
s.rawNode=_7.byId(_3e,_3b.ownerDocument).content.root;
_37[s._nodeName]=_3b;
s.onLoad(s);
}
};
if(_8("safari")){
obj="<embed type='application/x-silverlight' id='"+_3e+"' width='"+_3c+"' height='"+_3d+" background='transparent'"+" source='#"+t.id+"'"+" windowless='true'"+" maxFramerate='60'"+" onLoad='"+_3f+"'"+" onError='__dojoSilverlightError'"+" /><iframe style='visibility:hidden;height:0;width:0'/>";
}else{
obj="<object type='application/x-silverlight' data='data:application/x-silverlight,' id='"+_3e+"' width='"+_3c+"' height='"+_3d+"'>"+"<param name='background' value='transparent' />"+"<param name='source' value='#"+t.id+"' />"+"<param name='windowless' value='true' />"+"<param name='maxFramerate' value='60' />"+"<param name='onLoad' value='"+_3f+"' />"+"<param name='onError' value='__dojoSilverlightError' />"+"</object>";
}
_3b.innerHTML=obj;
var _41=_7.byId(_3e,_3b.ownerDocument);
if(_41.content&&_41.content.root){
s.rawNode=_41.content.root;
_37[s._nodeName]=_3b;
}else{
s.rawNode=null;
s.isLoaded=false;
}
s._nodes.push(_41);
s.width=g.normalizedLength(_3c);
s.height=g.normalizedLength(_3d);
return s;
};
__dojoSilverlightError=function(_42,err){
var t="Silverlight Error:\n"+"Code: "+err.ErrorCode+"\n"+"Type: "+err.ErrorType+"\n"+"Message: "+err.ErrorMessage+"\n";
switch(err.ErrorType){
case "ParserError":
t+="XamlFile: "+err.xamlFile+"\n"+"Line: "+err.lineNumber+"\n"+"Position: "+err.charPosition+"\n";
break;
case "RuntimeError":
t+="MethodName: "+err.methodName+"\n";
if(err.lineNumber!=0){
t+="Line: "+err.lineNumber+"\n"+"Position: "+err.charPosition+"\n";
}
break;
}
};
var _43={_setFont:function(){
var f=this.fontStyle,r=this.rawNode,t=f.family.toLowerCase();
r.fontStyle=f.style=="italic"?"Italic":"Normal";
r.fontWeight=f.weight in _b?_b[f.weight]:f.weight;
r.fontSize=g.normalizedLength(f.size);
r.fontFamily=t in _e?_e[t]:f.family;
if(!this._delay){
this._delay=window.setTimeout(_2.hitch(this,"_delayAlignment"),10);
}
}};
var C=gs.Container,_44={add:function(_45){
if(this!=_45.getParent()){
C.add.apply(this,arguments);
this.rawNode.children.add(_45.rawNode);
}
return this;
},remove:function(_46,_47){
if(this==_46.getParent()){
var _48=_46.rawNode.getParent();
if(_48){
_48.children.remove(_46.rawNode);
}
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
this.rawNode.children.clear();
return C.clear.apply(this,arguments);
},getBoundingBox:C.getBoundingBox,_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _49={createObject:function(_4a,_4b){
if(!this.rawNode){
return null;
}
var _4c=new _4a();
var _4d=this.rawNode.getHost().content.createFromXaml("<"+_4a.nodeType+"/>");
_4c.setRawNode(_4d);
_4c.setShape(_4b);
this.add(_4c);
return _4c;
}};
_2.extend(sl.Text,_43);
_2.extend(sl.Group,_44);
_2.extend(sl.Group,gs.Creator);
_2.extend(sl.Group,_49);
_2.extend(sl.Surface,_44);
_2.extend(sl.Surface,gs.Creator);
_2.extend(sl.Surface,_49);
function _4e(s,a){
var ev={target:s,currentTarget:s,preventDefault:function(){
},stopPropagation:function(){
}};
try{
if(a.source){
ev.target=a.source;
var _4f=ev.target.tag;
ev.gfxTarget=gs.byId(_4f);
}
}
catch(e){
}
if(a){
try{
ev.ctrlKey=a.ctrl;
ev.shiftKey=a.shift;
var p=a.getPosition(null);
ev.x=ev.offsetX=ev.layerX=p.x;
ev.y=ev.offsetY=ev.layerY=p.y;
var _50=_37[s.getHost().content.root.name];
var t=_6.position(_50);
ev.clientX=t.x+p.x;
ev.clientY=t.y+p.y;
}
catch(e){
}
}
return ev;
};
function _51(s,a){
var ev={keyCode:a.platformKeyCode,ctrlKey:a.ctrl,shiftKey:a.shift};
try{
if(a.source){
ev.target=a.source;
ev.gfxTarget=gs.byId(ev.target.tag);
}
}
catch(e){
}
return ev;
};
var _52={onclick:{name:"MouseLeftButtonUp",fix:_4e},onmouseenter:{name:"MouseEnter",fix:_4e},onmouseleave:{name:"MouseLeave",fix:_4e},onmouseover:{name:"MouseEnter",fix:_4e},onmouseout:{name:"MouseLeave",fix:_4e},onmousedown:{name:"MouseLeftButtonDown",fix:_4e},onmouseup:{name:"MouseLeftButtonUp",fix:_4e},onmousemove:{name:"MouseMove",fix:_4e},onkeydown:{name:"KeyDown",fix:_51},onkeyup:{name:"KeyUp",fix:_51}};
var _53={connect:function(_54,_55,_56){
return this.on(_54,_56?_2.hitch(_55,_56):_55);
},on:function(_57,_58){
if(typeof _57==="string"){
if(_57.indexOf("mouse")===0){
_57="on"+_57;
}
var _59,n=_57 in _52?_52[_57]:{name:_57,fix:function(){
return {};
}};
_59=this.getEventSource().addEventListener(n.name,function(s,a){
_58(n.fix(s,a));
});
return {name:n.name,token:_59,remove:_2.hitch(this,function(){
this.getEventSource().removeEventListener(n.name,_59);
})};
}else{
return on(this,_57,_58);
}
},disconnect:function(_5a){
return _5a.remove();
}};
_2.extend(sl.Shape,_53);
_2.extend(sl.Surface,_53);
g.equalSources=function(a,b){
return a&&b&&a.equals(b);
};
return sl;
});
