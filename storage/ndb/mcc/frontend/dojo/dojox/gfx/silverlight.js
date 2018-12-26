//>>built
define("dojox/gfx/silverlight",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","dojo/_base/array","dojo/dom-geometry","dojo/dom","dojo/_base/sniff","./_base","./shape","./path"],function(_1,_2,_3,_4,_5,_6,_7,_8,g,gs,_9){
var sl=g.silverlight={};
_1.experimental("dojox.gfx.silverlight");
var _a={solid:"none",shortdash:[4,1],shortdot:[1,1],shortdashdot:[4,1,1,1],shortdashdotdot:[4,1,1,1,1,1],dot:[1,3],dash:[4,3],longdash:[8,3],dashdot:[4,3,1,3],longdashdot:[8,3,1,3],longdashdotdot:[8,3,1,3,1,3]},_b={normal:400,bold:700},_c={butt:"Flat",round:"Round",square:"Square"},_d={bevel:"Bevel",round:"Round"},_e={serif:"Times New Roman",times:"Times New Roman","sans-serif":"Arial",helvetica:"Arial",monotone:"Courier New",courier:"Courier New"};
function _f(_10){
var c=g.normalizeColor(_10),t=c.toHex(),a=Math.round(c.a*255);
a=(a<0?0:a>255?255:a).toString(16);
return "#"+(a.length<2?"0"+a:a)+t.slice(1);
};
_3("dojox.gfx.silverlight.Shape",gs.Shape,{setFill:function(_11){
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
}});
_3("dojox.gfx.silverlight.Group",sl.Shape,{constructor:function(){
gs.Container._init.call(this);
},setRawNode:function(_15){
this.rawNode=_15;
this.rawNode.tag=this.getUID();
}});
sl.Group.nodeType="Canvas";
_3("dojox.gfx.silverlight.Rect",[sl.Shape,gs.Rect],{setShape:function(_16){
this.shape=g.makeParameters(this.shape,_16);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=n.width;
r.height=n.height;
r.radiusX=r.radiusY=n.r;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _17=this.matrix,s=this.shape,_18={dx:s.x,dy:s.y};
return new g.Matrix2D(_17?[_17,_18]:_18);
}});
sl.Rect.nodeType="Rectangle";
_3("dojox.gfx.silverlight.Ellipse",[sl.Shape,gs.Ellipse],{setShape:function(_19){
this.shape=g.makeParameters(this.shape,_19);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=2*n.rx;
r.height=2*n.ry;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _1a=this.matrix,s=this.shape,_1b={dx:s.cx-s.rx,dy:s.cy-s.ry};
return new g.Matrix2D(_1a?[_1a,_1b]:_1b);
}});
sl.Ellipse.nodeType="Ellipse";
_3("dojox.gfx.silverlight.Circle",[sl.Shape,gs.Circle],{setShape:function(_1c){
this.shape=g.makeParameters(this.shape,_1c);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=r.height=2*n.r;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _1d=this.matrix,s=this.shape,_1e={dx:s.cx-s.r,dy:s.cy-s.r};
return new g.Matrix2D(_1d?[_1d,_1e]:_1e);
}});
sl.Circle.nodeType="Ellipse";
_3("dojox.gfx.silverlight.Line",[sl.Shape,gs.Line],{setShape:function(_1f){
this.shape=g.makeParameters(this.shape,_1f);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.x1=n.x1;
r.y1=n.y1;
r.x2=n.x2;
r.y2=n.y2;
return this;
}});
sl.Line.nodeType="Line";
_3("dojox.gfx.silverlight.Polyline",[sl.Shape,gs.Polyline],{setShape:function(_20,_21){
if(_20&&_20 instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_20});
if(_21&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_20);
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
_3("dojox.gfx.silverlight.Image",[sl.Shape,gs.Image],{setShape:function(_22){
this.shape=g.makeParameters(this.shape,_22);
this.bbox=null;
var r=this.rawNode,n=this.shape;
r.width=n.width;
r.height=n.height;
r.source=n.src;
return this._applyTransform();
},_getAdjustedMatrix:function(){
var _23=this.matrix,s=this.shape,_24={dx:s.x,dy:s.y};
return new g.Matrix2D(_23?[_23,_24]:_24);
},setRawNode:function(_25){
this.rawNode=_25;
this.rawNode.tag=this.getUID();
}});
sl.Image.nodeType="Image";
_3("dojox.gfx.silverlight.Text",[sl.Shape,gs.Text],{setShape:function(_26){
this.shape=g.makeParameters(this.shape,_26);
this.bbox=null;
var r=this.rawNode,s=this.shape;
r.text=s.text;
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
var _27=this.matrix,_28=this._delta,x;
if(_27){
x=_28?[_27,_28]:_27;
}else{
x=_28?_28:{};
}
return new g.Matrix2D(x);
},setStroke:function(){
return this;
},_setFillAttr:function(f){
this.rawNode.foreground=f;
},setRawNode:function(_29){
this.rawNode=_29;
this.rawNode.tag=this.getUID();
},getTextWidth:function(){
return this.rawNode.actualWidth;
}});
sl.Text.nodeType="TextBlock";
_3("dojox.gfx.silverlight.Path",[sl.Shape,_9.Path],{_updateWithSegment:function(_2a){
this.inherited(arguments);
var p=this.shape.path;
if(typeof (p)=="string"){
this.rawNode.data=p?p:null;
}
},setShape:function(_2b){
this.inherited(arguments);
var p=this.shape.path;
this.rawNode.data=p?p:null;
return this;
}});
sl.Path.nodeType="Path";
_3("dojox.gfx.silverlight.TextPath",[sl.Shape,_9.TextPath],{_updateWithSegment:function(_2c){
},setShape:function(_2d){
},_setText:function(){
}});
sl.TextPath.nodeType="text";
var _2e={},_2f=new Function;
_3("dojox.gfx.silverlight.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
window[this._onLoadName]=_2f;
delete _2e[this._nodeName];
this.inherited(arguments);
},setDimensions:function(_30,_31){
this.width=g.normalizedLength(_30);
this.height=g.normalizedLength(_31);
var p=this.rawNode&&this.rawNode.getHost();
if(p){
p.width=_30;
p.height=_31;
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
sl.createSurface=function(_32,_33,_34){
if(!_33&&!_34){
var pos=_6.position(_32);
_33=_33||pos.w;
_34=_34||pos.h;
}
if(typeof _33=="number"){
_33=_33+"px";
}
if(typeof _34=="number"){
_34=_34+"px";
}
var s=new sl.Surface();
_32=_7.byId(_32);
s._parent=_32;
s._nodeName=g._base._getUniqueId();
var t=_32.ownerDocument.createElement("script");
t.type="text/xaml";
t.id=g._base._getUniqueId();
t.text="<?xml version='1.0'?><Canvas xmlns='http://schemas.microsoft.com/client/2007' Name='"+s._nodeName+"'/>";
_32.parentNode.insertBefore(t,_32);
s._nodes.push(t);
var obj,_35=g._base._getUniqueId(),_36="__"+g._base._getUniqueId()+"_onLoad";
s._onLoadName=_36;
window[_36]=function(_37){
if(!s.rawNode){
s.rawNode=_7.byId(_35).content.root;
_2e[s._nodeName]=_32;
s.onLoad(s);
}
};
if(_8("safari")){
obj="<embed type='application/x-silverlight' id='"+_35+"' width='"+_33+"' height='"+_34+" background='transparent'"+" source='#"+t.id+"'"+" windowless='true'"+" maxFramerate='60'"+" onLoad='"+_36+"'"+" onError='__dojoSilverlightError'"+" /><iframe style='visibility:hidden;height:0;width:0'/>";
}else{
obj="<object type='application/x-silverlight' data='data:application/x-silverlight,' id='"+_35+"' width='"+_33+"' height='"+_34+"'>"+"<param name='background' value='transparent' />"+"<param name='source' value='#"+t.id+"' />"+"<param name='windowless' value='true' />"+"<param name='maxFramerate' value='60' />"+"<param name='onLoad' value='"+_36+"' />"+"<param name='onError' value='__dojoSilverlightError' />"+"</object>";
}
_32.innerHTML=obj;
var _38=_7.byId(_35);
if(_38.content&&_38.content.root){
s.rawNode=_38.content.root;
_2e[s._nodeName]=_32;
}else{
s.rawNode=null;
s.isLoaded=false;
}
s._nodes.push(_38);
s.width=g.normalizedLength(_33);
s.height=g.normalizedLength(_34);
return s;
};
__dojoSilverlightError=function(_39,err){
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
var _3a={_setFont:function(){
var f=this.fontStyle,r=this.rawNode,t=f.family.toLowerCase();
r.fontStyle=f.style=="italic"?"Italic":"Normal";
r.fontWeight=f.weight in _b?_b[f.weight]:f.weight;
r.fontSize=g.normalizedLength(f.size);
r.fontFamily=t in _e?_e[t]:f.family;
if(!this._delay){
this._delay=window.setTimeout(_2.hitch(this,"_delayAlignment"),10);
}
}};
var C=gs.Container,_3b={add:function(_3c){
if(this!=_3c.getParent()){
C.add.apply(this,arguments);
this.rawNode.children.add(_3c.rawNode);
}
return this;
},remove:function(_3d,_3e){
if(this==_3d.getParent()){
var _3f=_3d.rawNode.getParent();
if(_3f){
_3f.children.remove(_3d.rawNode);
}
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
this.rawNode.children.clear();
return C.clear.apply(this,arguments);
},_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _40={createObject:function(_41,_42){
if(!this.rawNode){
return null;
}
var _43=new _41();
var _44=this.rawNode.getHost().content.createFromXaml("<"+_41.nodeType+"/>");
_43.setRawNode(_44);
_43.setShape(_42);
this.add(_43);
return _43;
}};
_2.extend(sl.Text,_3a);
_2.extend(sl.Group,_3b);
_2.extend(sl.Group,gs.Creator);
_2.extend(sl.Group,_40);
_2.extend(sl.Surface,_3b);
_2.extend(sl.Surface,gs.Creator);
_2.extend(sl.Surface,_40);
function _45(s,a){
var ev={target:s,currentTarget:s,preventDefault:function(){
},stopPropagation:function(){
}};
try{
if(a.source){
ev.target=a.source;
var _46=ev.target.tag;
ev.gfxTarget=gs.byId(_46);
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
var _47=_2e[s.getHost().content.root.name];
var t=_6.position(_47);
ev.clientX=t.x+p.x;
ev.clientY=t.y+p.y;
}
catch(e){
}
}
return ev;
};
function _48(s,a){
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
var _49={onclick:{name:"MouseLeftButtonUp",fix:_45},onmouseenter:{name:"MouseEnter",fix:_45},onmouseleave:{name:"MouseLeave",fix:_45},onmouseover:{name:"MouseEnter",fix:_45},onmouseout:{name:"MouseLeave",fix:_45},onmousedown:{name:"MouseLeftButtonDown",fix:_45},onmouseup:{name:"MouseLeftButtonUp",fix:_45},onmousemove:{name:"MouseMove",fix:_45},onkeydown:{name:"KeyDown",fix:_48},onkeyup:{name:"KeyUp",fix:_48}};
var _4a={connect:function(_4b,_4c,_4d){
var _4e,n=_4b in _49?_49[_4b]:{name:_4b,fix:function(){
return {};
}};
if(arguments.length>2){
_4e=this.getEventSource().addEventListener(n.name,function(s,a){
_2.hitch(_4c,_4d)(n.fix(s,a));
});
}else{
_4e=this.getEventSource().addEventListener(n.name,function(s,a){
_4c(n.fix(s,a));
});
}
return {name:n.name,token:_4e};
},disconnect:function(_4f){
try{
this.getEventSource().removeEventListener(_4f.name,_4f.token);
}
catch(e){
}
}};
_2.extend(sl.Shape,_4a);
_2.extend(sl.Surface,_4a);
g.equalSources=function(a,b){
return a&&b&&a.equals(b);
};
return sl;
});
