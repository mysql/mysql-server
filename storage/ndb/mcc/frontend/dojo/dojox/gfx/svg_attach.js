//>>built
define("dojox/gfx/svg_attach",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/Color","./_base","./svg","./matrix"],function(_1,_2,_3,_4,g,_5,_6){
_1.experimental("dojox.gfx.svg_attach");
_5.attachNode=function(_7){
if(!_7){
return null;
}
var s=null;
switch(_7.tagName.toLowerCase()){
case _5.Rect.nodeType:
s=new _5.Rect(_7);
_8(s);
break;
case _5.Ellipse.nodeType:
s=new _5.Ellipse(_7);
_9(s,g.defaultEllipse);
break;
case _5.Polyline.nodeType:
s=new _5.Polyline(_7);
_9(s,g.defaultPolyline);
break;
case _5.Path.nodeType:
s=new _5.Path(_7);
_9(s,g.defaultPath);
break;
case _5.Circle.nodeType:
s=new _5.Circle(_7);
_9(s,g.defaultCircle);
break;
case _5.Line.nodeType:
s=new _5.Line(_7);
_9(s,g.defaultLine);
break;
case _5.Image.nodeType:
s=new _5.Image(_7);
_9(s,g.defaultImage);
break;
case _5.Text.nodeType:
var t=_7.getElementsByTagName("textPath");
if(t&&t.length){
s=new _5.TextPath(_7);
_9(s,g.defaultPath);
_a(s);
}else{
s=new _5.Text(_7);
_b(s);
}
_c(s);
break;
default:
return null;
}
if(!(s instanceof _5.Image)){
_d(s);
_e(s);
}
_f(s);
return s;
};
_5.attachSurface=function(_10){
var s=new _5.Surface();
s.rawNode=_10;
var _11=_10.getElementsByTagName("defs");
if(_11.length==0){
return null;
}
s.defNode=_11[0];
return s;
};
function _d(_12){
var _13=_12.rawNode.getAttribute("fill");
if(_13=="none"){
_12.fillStyle=null;
return;
}
var _14=null,_15=_5.getRef(_13);
if(_15){
switch(_15.tagName.toLowerCase()){
case "lineargradient":
_14=_16(g.defaultLinearGradient,_15);
_3.forEach(["x1","y1","x2","y2"],function(x){
_14[x]=_15.getAttribute(x);
});
break;
case "radialgradient":
_14=_16(g.defaultRadialGradient,_15);
_3.forEach(["cx","cy","r"],function(x){
_14[x]=_15.getAttribute(x);
});
_14.cx=_15.getAttribute("cx");
_14.cy=_15.getAttribute("cy");
_14.r=_15.getAttribute("r");
break;
case "pattern":
_14=_2.clone(g.defaultPattern);
_3.forEach(["x","y","width","height"],function(x){
_14[x]=_15.getAttribute(x);
});
_14.src=_15.firstChild.getAttributeNS(_5.xmlns.xlink,"href");
break;
}
}else{
_14=new _4(_13);
var _17=_12.rawNode.getAttribute("fill-opacity");
if(_17!=null){
_14.a=_17;
}
}
_12.fillStyle=_14;
};
function _16(_18,_19){
var _1a=_2.clone(_18);
_1a.colors=[];
for(var i=0;i<_19.childNodes.length;++i){
_1a.colors.push({offset:_19.childNodes[i].getAttribute("offset"),color:new _4(_19.childNodes[i].getAttribute("stop-color"))});
}
return _1a;
};
function _e(_1b){
var _1c=_1b.rawNode,_1d=_1c.getAttribute("stroke");
if(_1d==null||_1d=="none"){
_1b.strokeStyle=null;
return;
}
var _1e=_1b.strokeStyle=_2.clone(g.defaultStroke);
var _1f=new _4(_1d);
if(_1f){
_1e.color=_1f;
_1e.color.a=_1c.getAttribute("stroke-opacity");
_1e.width=_1c.getAttribute("stroke-width");
_1e.cap=_1c.getAttribute("stroke-linecap");
_1e.join=_1c.getAttribute("stroke-linejoin");
if(_1e.join=="miter"){
_1e.join=_1c.getAttribute("stroke-miterlimit");
}
_1e.style=_1c.getAttribute("dojoGfxStrokeStyle");
}
};
function _f(_20){
var _21=_20.rawNode.getAttribute("transform");
if(_21.match(/^matrix\(.+\)$/)){
var t=_21.slice(7,-1).split(",");
_20.matrix=_6.normalize({xx:parseFloat(t[0]),xy:parseFloat(t[2]),yx:parseFloat(t[1]),yy:parseFloat(t[3]),dx:parseFloat(t[4]),dy:parseFloat(t[5])});
}else{
_20.matrix=null;
}
};
function _c(_22){
var _23=_22.fontStyle=_2.clone(g.defaultFont),r=_22.rawNode;
_23.style=r.getAttribute("font-style");
_23.variant=r.getAttribute("font-variant");
_23.weight=r.getAttribute("font-weight");
_23.size=r.getAttribute("font-size");
_23.family=r.getAttribute("font-family");
};
function _9(_24,def){
var _25=_24.shape=_2.clone(def),r=_24.rawNode;
for(var i in _25){
_25[i]=r.getAttribute(i);
}
};
function _8(_26){
_9(_26,g.defaultRect);
_26.shape.r=Math.min(_26.rawNode.getAttribute("rx"),_26.rawNode.getAttribute("ry"));
};
function _b(_27){
var _28=_27.shape=_2.clone(g.defaultText),r=_27.rawNode;
_28.x=r.getAttribute("x");
_28.y=r.getAttribute("y");
_28.align=r.getAttribute("text-anchor");
_28.decoration=r.getAttribute("text-decoration");
_28.rotated=parseFloat(r.getAttribute("rotate"))!=0;
_28.kerning=r.getAttribute("kerning")=="auto";
_28.text=r.firstChild.nodeValue;
};
function _a(_29){
var _2a=_29.shape=_2.clone(g.defaultTextPath),r=_29.rawNode;
_2a.align=r.getAttribute("text-anchor");
_2a.decoration=r.getAttribute("text-decoration");
_2a.rotated=parseFloat(r.getAttribute("rotate"))!=0;
_2a.kerning=r.getAttribute("kerning")=="auto";
_2a.text=r.firstChild.nodeValue;
};
return _5;
});
