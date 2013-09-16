//>>built
define("dojox/gfx/svg",["dojo/_base/lang","dojo/_base/window","dojo/dom","dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","dojo/_base/Color","./_base","./shape","./path"],function(_1,_2,_3,_4,_5,_6,_7,g,gs,_8){
var _9=g.svg={};
_9.useSvgWeb=(typeof window.svgweb!="undefined");
var _a=navigator.userAgent.toLowerCase(),_b=_a.search("iphone")>-1||_a.search("ipad")>-1||_a.search("ipod")>-1;
function _c(ns,_d){
if(_2.doc.createElementNS){
return _2.doc.createElementNS(ns,_d);
}else{
return _2.doc.createElement(_d);
}
};
function _e(_f){
if(_9.useSvgWeb){
return _2.doc.createTextNode(_f,true);
}else{
return _2.doc.createTextNode(_f);
}
};
function _10(){
if(_9.useSvgWeb){
return _2.doc.createDocumentFragment(true);
}else{
return _2.doc.createDocumentFragment();
}
};
_9.xmlns={xlink:"http://www.w3.org/1999/xlink",svg:"http://www.w3.org/2000/svg"};
_9.getRef=function(_11){
if(!_11||_11=="none"){
return null;
}
if(_11.match(/^url\(#.+\)$/)){
return _3.byId(_11.slice(5,-1));
}
if(_11.match(/^#dojoUnique\d+$/)){
return _3.byId(_11.slice(1));
}
return null;
};
_9.dasharray={solid:"none",shortdash:[4,1],shortdot:[1,1],shortdashdot:[4,1,1,1],shortdashdotdot:[4,1,1,1,1,1],dot:[1,3],dash:[4,3],longdash:[8,3],dashdot:[4,3,1,3],longdashdot:[8,3,1,3],longdashdotdot:[8,3,1,3,1,3]};
_4("dojox.gfx.svg.Shape",gs.Shape,{setFill:function(_12){
if(!_12){
this.fillStyle=null;
this.rawNode.setAttribute("fill","none");
this.rawNode.setAttribute("fill-opacity",0);
return this;
}
var f;
var _13=function(x){
this.setAttribute(x,f[x].toFixed(8));
};
if(typeof (_12)=="object"&&"type" in _12){
switch(_12.type){
case "linear":
f=g.makeParameters(g.defaultLinearGradient,_12);
var _14=this._setFillObject(f,"linearGradient");
_5.forEach(["x1","y1","x2","y2"],_13,_14);
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_12);
var _15=this._setFillObject(f,"radialGradient");
_5.forEach(["cx","cy","r"],_13,_15);
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_12);
var _16=this._setFillObject(f,"pattern");
_5.forEach(["x","y","width","height"],_13,_16);
break;
}
this.fillStyle=f;
return this;
}
f=g.normalizeColor(_12);
this.fillStyle=f;
this.rawNode.setAttribute("fill",f.toCss());
this.rawNode.setAttribute("fill-opacity",f.a);
this.rawNode.setAttribute("fill-rule","evenodd");
return this;
},setStroke:function(_17){
var rn=this.rawNode;
if(!_17){
this.strokeStyle=null;
rn.setAttribute("stroke","none");
rn.setAttribute("stroke-opacity",0);
return this;
}
if(typeof _17=="string"||_1.isArray(_17)||_17 instanceof _7){
_17={color:_17};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_17);
s.color=g.normalizeColor(s.color);
if(s){
rn.setAttribute("stroke",s.color.toCss());
rn.setAttribute("stroke-opacity",s.color.a);
rn.setAttribute("stroke-width",s.width);
rn.setAttribute("stroke-linecap",s.cap);
if(typeof s.join=="number"){
rn.setAttribute("stroke-linejoin","miter");
rn.setAttribute("stroke-miterlimit",s.join);
}else{
rn.setAttribute("stroke-linejoin",s.join);
}
var da=s.style.toLowerCase();
if(da in _9.dasharray){
da=_9.dasharray[da];
}
if(da instanceof Array){
da=_1._toArray(da);
for(var i=0;i<da.length;++i){
da[i]*=s.width;
}
if(s.cap!="butt"){
for(var i=0;i<da.length;i+=2){
da[i]-=s.width;
if(da[i]<1){
da[i]=1;
}
}
for(var i=1;i<da.length;i+=2){
da[i]+=s.width;
}
}
da=da.join(",");
}
rn.setAttribute("stroke-dasharray",da);
rn.setAttribute("dojoGfxStrokeStyle",s.style);
}
return this;
},_getParentSurface:function(){
var _18=this.parent;
for(;_18&&!(_18 instanceof g.Surface);_18=_18.parent){
}
return _18;
},_setFillObject:function(f,_19){
var _1a=_9.xmlns.svg;
this.fillStyle=f;
var _1b=this._getParentSurface(),_1c=_1b.defNode,_1d=this.rawNode.getAttribute("fill"),ref=_9.getRef(_1d);
if(ref){
_1d=ref;
if(_1d.tagName.toLowerCase()!=_19.toLowerCase()){
var id=_1d.id;
_1d.parentNode.removeChild(_1d);
_1d=_c(_1a,_19);
_1d.setAttribute("id",id);
_1c.appendChild(_1d);
}else{
while(_1d.childNodes.length){
_1d.removeChild(_1d.lastChild);
}
}
}else{
_1d=_c(_1a,_19);
_1d.setAttribute("id",g._base._getUniqueId());
_1c.appendChild(_1d);
}
if(_19=="pattern"){
_1d.setAttribute("patternUnits","userSpaceOnUse");
var img=_c(_1a,"image");
img.setAttribute("x",0);
img.setAttribute("y",0);
img.setAttribute("width",f.width.toFixed(8));
img.setAttribute("height",f.height.toFixed(8));
img.setAttributeNS(_9.xmlns.xlink,"xlink:href",f.src);
_1d.appendChild(img);
}else{
_1d.setAttribute("gradientUnits","userSpaceOnUse");
for(var i=0;i<f.colors.length;++i){
var c=f.colors[i],t=_c(_1a,"stop"),cc=c.color=g.normalizeColor(c.color);
t.setAttribute("offset",c.offset.toFixed(8));
t.setAttribute("stop-color",cc.toCss());
t.setAttribute("stop-opacity",cc.a);
_1d.appendChild(t);
}
}
this.rawNode.setAttribute("fill","url(#"+_1d.getAttribute("id")+")");
this.rawNode.removeAttribute("fill-opacity");
this.rawNode.setAttribute("fill-rule","evenodd");
return _1d;
},_applyTransform:function(){
var _1e=this.matrix;
if(_1e){
var tm=this.matrix;
this.rawNode.setAttribute("transform","matrix("+tm.xx.toFixed(8)+","+tm.yx.toFixed(8)+","+tm.xy.toFixed(8)+","+tm.yy.toFixed(8)+","+tm.dx.toFixed(8)+","+tm.dy.toFixed(8)+")");
}else{
this.rawNode.removeAttribute("transform");
}
return this;
},setRawNode:function(_1f){
var r=this.rawNode=_1f;
if(this.shape.type!="image"){
r.setAttribute("fill","none");
}
r.setAttribute("fill-opacity",0);
r.setAttribute("stroke","none");
r.setAttribute("stroke-opacity",0);
r.setAttribute("stroke-width",1);
r.setAttribute("stroke-linecap","butt");
r.setAttribute("stroke-linejoin","miter");
r.setAttribute("stroke-miterlimit",4);
r.__gfxObject__=this.getUID();
},setShape:function(_20){
this.shape=g.makeParameters(this.shape,_20);
for(var i in this.shape){
if(i!="type"){
this.rawNode.setAttribute(i,this.shape[i]);
}
}
this.bbox=null;
return this;
},_moveToFront:function(){
this.rawNode.parentNode.appendChild(this.rawNode);
return this;
},_moveToBack:function(){
this.rawNode.parentNode.insertBefore(this.rawNode,this.rawNode.parentNode.firstChild);
return this;
}});
_4("dojox.gfx.svg.Group",_9.Shape,{constructor:function(){
gs.Container._init.call(this);
},setRawNode:function(_21){
this.rawNode=_21;
this.rawNode.__gfxObject__=this.getUID();
}});
_9.Group.nodeType="g";
_4("dojox.gfx.svg.Rect",[_9.Shape,gs.Rect],{setShape:function(_22){
this.shape=g.makeParameters(this.shape,_22);
this.bbox=null;
for(var i in this.shape){
if(i!="type"&&i!="r"){
this.rawNode.setAttribute(i,this.shape[i]);
}
}
if(this.shape.r!=null){
this.rawNode.setAttribute("ry",this.shape.r);
this.rawNode.setAttribute("rx",this.shape.r);
}
return this;
}});
_9.Rect.nodeType="rect";
_4("dojox.gfx.svg.Ellipse",[_9.Shape,gs.Ellipse],{});
_9.Ellipse.nodeType="ellipse";
_4("dojox.gfx.svg.Circle",[_9.Shape,gs.Circle],{});
_9.Circle.nodeType="circle";
_4("dojox.gfx.svg.Line",[_9.Shape,gs.Line],{});
_9.Line.nodeType="line";
_4("dojox.gfx.svg.Polyline",[_9.Shape,gs.Polyline],{setShape:function(_23,_24){
if(_23&&_23 instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_23});
if(_24&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_23);
}
this.bbox=null;
this._normalizePoints();
var _25=[],p=this.shape.points;
for(var i=0;i<p.length;++i){
_25.push(p[i].x.toFixed(8),p[i].y.toFixed(8));
}
this.rawNode.setAttribute("points",_25.join(" "));
return this;
}});
_9.Polyline.nodeType="polyline";
_4("dojox.gfx.svg.Image",[_9.Shape,gs.Image],{setShape:function(_26){
this.shape=g.makeParameters(this.shape,_26);
this.bbox=null;
var _27=this.rawNode;
for(var i in this.shape){
if(i!="type"&&i!="src"){
_27.setAttribute(i,this.shape[i]);
}
}
_27.setAttribute("preserveAspectRatio","none");
_27.setAttributeNS(_9.xmlns.xlink,"xlink:href",this.shape.src);
_27.__gfxObject__=this.getUID();
return this;
}});
_9.Image.nodeType="image";
_4("dojox.gfx.svg.Text",[_9.Shape,gs.Text],{setShape:function(_28){
this.shape=g.makeParameters(this.shape,_28);
this.bbox=null;
var r=this.rawNode,s=this.shape;
r.setAttribute("x",s.x);
r.setAttribute("y",s.y);
r.setAttribute("text-anchor",s.align);
r.setAttribute("text-decoration",s.decoration);
r.setAttribute("rotate",s.rotated?90:0);
r.setAttribute("kerning",s.kerning?"auto":0);
r.setAttribute("text-rendering","optimizeLegibility");
if(r.firstChild){
r.firstChild.nodeValue=s.text;
}else{
r.appendChild(_e(s.text));
}
return this;
},getTextWidth:function(){
var _29=this.rawNode,_2a=_29.parentNode,_2b=_29.cloneNode(true);
_2b.style.visibility="hidden";
var _2c=0,_2d=_2b.firstChild.nodeValue;
_2a.appendChild(_2b);
if(_2d!=""){
while(!_2c){
if(_2b.getBBox){
_2c=parseInt(_2b.getBBox().width);
}else{
_2c=68;
}
}
}
_2a.removeChild(_2b);
return _2c;
}});
_9.Text.nodeType="text";
_4("dojox.gfx.svg.Path",[_9.Shape,_8.Path],{_updateWithSegment:function(_2e){
this.inherited(arguments);
if(typeof (this.shape.path)=="string"){
this.rawNode.setAttribute("d",this.shape.path);
}
},setShape:function(_2f){
this.inherited(arguments);
if(this.shape.path){
this.rawNode.setAttribute("d",this.shape.path);
}else{
this.rawNode.removeAttribute("d");
}
return this;
}});
_9.Path.nodeType="path";
_4("dojox.gfx.svg.TextPath",[_9.Shape,_8.TextPath],{_updateWithSegment:function(_30){
this.inherited(arguments);
this._setTextPath();
},setShape:function(_31){
this.inherited(arguments);
this._setTextPath();
return this;
},_setTextPath:function(){
if(typeof this.shape.path!="string"){
return;
}
var r=this.rawNode;
if(!r.firstChild){
var tp=_c(_9.xmlns.svg,"textPath"),tx=_e("");
tp.appendChild(tx);
r.appendChild(tp);
}
var ref=r.firstChild.getAttributeNS(_9.xmlns.xlink,"href"),_32=ref&&_9.getRef(ref);
if(!_32){
var _33=this._getParentSurface();
if(_33){
var _34=_33.defNode;
_32=_c(_9.xmlns.svg,"path");
var id=g._base._getUniqueId();
_32.setAttribute("id",id);
_34.appendChild(_32);
r.firstChild.setAttributeNS(_9.xmlns.xlink,"xlink:href","#"+id);
}
}
if(_32){
_32.setAttribute("d",this.shape.path);
}
},_setText:function(){
var r=this.rawNode;
if(!r.firstChild){
var tp=_c(_9.xmlns.svg,"textPath"),tx=_e("");
tp.appendChild(tx);
r.appendChild(tp);
}
r=r.firstChild;
var t=this.text;
r.setAttribute("alignment-baseline","middle");
switch(t.align){
case "middle":
r.setAttribute("text-anchor","middle");
r.setAttribute("startOffset","50%");
break;
case "end":
r.setAttribute("text-anchor","end");
r.setAttribute("startOffset","100%");
break;
default:
r.setAttribute("text-anchor","start");
r.setAttribute("startOffset","0%");
break;
}
r.setAttribute("baseline-shift","0.5ex");
r.setAttribute("text-decoration",t.decoration);
r.setAttribute("rotate",t.rotated?90:0);
r.setAttribute("kerning",t.kerning?"auto":0);
r.firstChild.data=t.text;
}});
_9.TextPath.nodeType="text";
_4("dojox.gfx.svg.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
this.defNode=null;
this.inherited(arguments);
},setDimensions:function(_35,_36){
if(!this.rawNode){
return this;
}
this.rawNode.setAttribute("width",_35);
this.rawNode.setAttribute("height",_36);
return this;
},getDimensions:function(){
var t=this.rawNode?{width:g.normalizedLength(this.rawNode.getAttribute("width")),height:g.normalizedLength(this.rawNode.getAttribute("height"))}:null;
return t;
}});
_9.createSurface=function(_37,_38,_39){
var s=new _9.Surface();
s.rawNode=_c(_9.xmlns.svg,"svg");
s.rawNode.setAttribute("overflow","hidden");
if(_38){
s.rawNode.setAttribute("width",_38);
}
if(_39){
s.rawNode.setAttribute("height",_39);
}
var _3a=_c(_9.xmlns.svg,"defs");
s.rawNode.appendChild(_3a);
s.defNode=_3a;
s._parent=_3.byId(_37);
s._parent.appendChild(s.rawNode);
return s;
};
var _3b={_setFont:function(){
var f=this.fontStyle;
this.rawNode.setAttribute("font-style",f.style);
this.rawNode.setAttribute("font-variant",f.variant);
this.rawNode.setAttribute("font-weight",f.weight);
this.rawNode.setAttribute("font-size",f.size);
this.rawNode.setAttribute("font-family",f.family);
}};
var C=gs.Container,_3c={openBatch:function(){
this.fragment=_10();
},closeBatch:function(){
if(this.fragment){
this.rawNode.appendChild(this.fragment);
delete this.fragment;
}
},add:function(_3d){
if(this!=_3d.getParent()){
if(this.fragment){
this.fragment.appendChild(_3d.rawNode);
}else{
this.rawNode.appendChild(_3d.rawNode);
}
C.add.apply(this,arguments);
}
return this;
},remove:function(_3e,_3f){
if(this==_3e.getParent()){
if(this.rawNode==_3e.rawNode.parentNode){
this.rawNode.removeChild(_3e.rawNode);
}
if(this.fragment&&this.fragment==_3e.rawNode.parentNode){
this.fragment.removeChild(_3e.rawNode);
}
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
var r=this.rawNode;
while(r.lastChild){
r.removeChild(r.lastChild);
}
var _40=this.defNode;
if(_40){
while(_40.lastChild){
_40.removeChild(_40.lastChild);
}
r.appendChild(_40);
}
return C.clear.apply(this,arguments);
},_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _41={createObject:function(_42,_43){
if(!this.rawNode){
return null;
}
var _44=new _42(),_45=_c(_9.xmlns.svg,_42.nodeType);
_44.setRawNode(_45);
_44.setShape(_43);
this.add(_44);
return _44;
}};
_1.extend(_9.Text,_3b);
_1.extend(_9.TextPath,_3b);
_1.extend(_9.Group,_3c);
_1.extend(_9.Group,gs.Creator);
_1.extend(_9.Group,_41);
_1.extend(_9.Surface,_3c);
_1.extend(_9.Surface,gs.Creator);
_1.extend(_9.Surface,_41);
_9.fixTarget=function(_46,_47){
if(!_46.gfxTarget){
if(_b&&_46.target.wholeText){
_46.gfxTarget=gs.byId(_46.target.parentElement.__gfxObject__);
}else{
_46.gfxTarget=gs.byId(_46.target.__gfxObject__);
}
}
return true;
};
if(_9.useSvgWeb){
_9.createSurface=function(_48,_49,_4a){
var s=new _9.Surface();
if(!_49||!_4a){
var pos=_6.position(_48);
_49=_49||pos.w;
_4a=_4a||pos.h;
}
_48=_3.byId(_48);
var id=_48.id?_48.id+"_svgweb":g._base._getUniqueId();
var _4b=_c(_9.xmlns.svg,"svg");
_4b.id=id;
_4b.setAttribute("width",_49);
_4b.setAttribute("height",_4a);
svgweb.appendChild(_4b,_48);
_4b.addEventListener("SVGLoad",function(){
s.rawNode=this;
s.isLoaded=true;
var _4c=_c(_9.xmlns.svg,"defs");
s.rawNode.appendChild(_4c);
s.defNode=_4c;
if(s.onLoad){
s.onLoad(s);
}
},false);
s.isLoaded=false;
return s;
};
_9.Surface.extend({destroy:function(){
var _4d=this.rawNode;
svgweb.removeChild(_4d,_4d.parentNode);
}});
var _4e={connect:function(_4f,_50,_51){
if(_4f.substring(0,2)==="on"){
_4f=_4f.substring(2);
}
if(arguments.length==2){
_51=_50;
}else{
_51=_1.hitch(_50,_51);
}
this.getEventSource().addEventListener(_4f,_51,false);
return [this,_4f,_51];
},disconnect:function(_52){
this.getEventSource().removeEventListener(_52[1],_52[2],false);
delete _52[0];
}};
_1.extend(_9.Shape,_4e);
_1.extend(_9.Surface,_4e);
}
return _9;
});
