//>>built
define("dojox/gfx/vml_attach",["dojo/_base/kernel","dojo/_base/lang","./_base","./matrix","./path","dojo/_base/Color","./vml"],function(_1,_2,g,m,_3,_4,_5){
_1.experimental("dojox.gfx.vml_attach");
_5.attachNode=function(_6){
if(!_6){
return null;
}
var s=null;
switch(_6.tagName.toLowerCase()){
case _5.Rect.nodeType:
s=new _5.Rect(_6);
_7(s);
break;
case _5.Ellipse.nodeType:
if(_6.style.width==_6.style.height){
s=new _5.Circle(_6);
_8(s);
}else{
s=new _5.Ellipse(_6);
_9(s);
}
break;
case _5.Path.nodeType:
switch(_6.getAttribute("dojoGfxType")){
case "line":
s=new _5.Line(_6);
_a(s);
break;
case "polyline":
s=new _5.Polyline(_6);
_b(s);
break;
case "path":
s=new _5.Path(_6);
_c(s);
break;
case "text":
s=new _5.Text(_6);
_d(s);
_e(s);
_f(s);
break;
case "textpath":
s=new _5.TextPath(_6);
_c(s);
_d(s);
_e(s);
break;
}
break;
case _5.Image.nodeType:
switch(_6.getAttribute("dojoGfxType")){
case "image":
s=new _5.Image(_6);
_10(s);
_11(s);
break;
}
break;
default:
return null;
}
if(!(s instanceof _5.Image)){
_12(s);
_13(s);
if(!(s instanceof _5.Text)){
_14(s);
}
}
return s;
};
_5.attachSurface=function(_15){
var s=new _5.Surface();
s.clipNode=_15;
var r=s.rawNode=_15.firstChild;
var b=r.firstChild;
if(!b||b.tagName!="rect"){
return null;
}
s.bgNode=r;
return s;
};
var _12=function(_16){
var _17=null,r=_16.rawNode,fo=r.fill,_18,i,t;
if(fo.on&&fo.type=="gradient"){
_17=_2.clone(g.defaultLinearGradient),rad=m._degToRad(fo.angle);
_17.x2=Math.cos(rad);
_17.y2=Math.sin(rad);
_17.colors=[];
_18=fo.colors.value.split(";");
for(i=0;i<_18.length;++i){
t=_18[i].match(/\S+/g);
if(!t||t.length!=2){
continue;
}
_17.colors.push({offset:_5._parseFloat(t[0]),color:new _4(t[1])});
}
}else{
if(fo.on&&fo.type=="gradientradial"){
_17=_2.clone(g.defaultRadialGradient),w=parseFloat(r.style.width),h=parseFloat(r.style.height);
_17.cx=isNaN(w)?0:fo.focusposition.x*w;
_17.cy=isNaN(h)?0:fo.focusposition.y*h;
_17.r=isNaN(w)?1:w/2;
_17.colors=[];
_18=fo.colors.value.split(";");
for(i=_18.length-1;i>=0;--i){
t=_18[i].match(/\S+/g);
if(!t||t.length!=2){
continue;
}
_17.colors.push({offset:_5._parseFloat(t[0]),color:new _4(t[1])});
}
}else{
if(fo.on&&fo.type=="tile"){
_17=_2.clone(g.defaultPattern);
_17.width=g.pt2px(fo.size.x);
_17.height=g.pt2px(fo.size.y);
_17.x=fo.origin.x*_17.width;
_17.y=fo.origin.y*_17.height;
_17.src=fo.src;
}else{
if(fo.on&&r.fillcolor){
_17=new _4(r.fillcolor+"");
_17.a=fo.opacity;
}
}
}
}
_16.fillStyle=_17;
};
var _13=function(_19){
var r=_19.rawNode;
if(!r.stroked){
_19.strokeStyle=null;
return;
}
var _1a=_19.strokeStyle=_2.clone(g.defaultStroke),rs=r.stroke;
_1a.color=new _4(r.strokecolor.value);
_1a.width=g.normalizedLength(r.strokeweight+"");
_1a.color.a=rs.opacity;
_1a.cap=this._translate(this._capMapReversed,rs.endcap);
_1a.join=rs.joinstyle=="miter"?rs.miterlimit:rs.joinstyle;
_1a.style=rs.dashstyle;
};
var _14=function(_1b){
var s=_1b.rawNode.skew,sm=s.matrix,so=s.offset;
_1b.matrix=m.normalize({xx:sm.xtox,xy:sm.ytox,yx:sm.xtoy,yy:sm.ytoy,dx:g.pt2px(so.x),dy:g.pt2px(so.y)});
};
var _1c=function(_1d){
_1d.bgNode=_1d.rawNode.firstChild;
};
var _7=function(_1e){
var r=_1e.rawNode,_1f=r.outerHTML.match(/arcsize = \"(\d*\.?\d+[%f]?)\"/)[1],_20=r.style,_21=parseFloat(_20.width),_22=parseFloat(_20.height);
_1f=(_1f.indexOf("%")>=0)?parseFloat(_1f)/100:_5._parseFloat(_1f);
_1e.shape=g.makeParameters(g.defaultRect,{x:parseInt(_20.left),y:parseInt(_20.top),width:_21,height:_22,r:Math.min(_21,_22)*_1f});
};
var _9=function(_23){
var _24=_23.rawNode.style,rx=parseInt(_24.width)/2,ry=parseInt(_24.height)/2;
_23.shape=g.makeParameters(g.defaultEllipse,{cx:parseInt(_24.left)+rx,cy:parseInt(_24.top)+ry,rx:rx,ry:ry});
};
var _8=function(_25){
var _26=_25.rawNode.style,r=parseInt(_26.width)/2;
_25.shape=g.makeParameters(g.defaultCircle,{cx:parseInt(_26.left)+r,cy:parseInt(_26.top)+r,r:r});
};
var _a=function(_27){
var _28=_27.shape=_2.clone(g.defaultLine),p=_27.rawNode.path.v.match(g.pathVmlRegExp);
do{
if(p.length<7||p[0]!="m"||p[3]!="l"||p[6]!="e"){
break;
}
_28.x1=parseInt(p[1]);
_28.y1=parseInt(p[2]);
_28.x2=parseInt(p[4]);
_28.y2=parseInt(p[5]);
}while(false);
};
var _b=function(_29){
var _2a=_29.shape=_2.clone(g.defaultPolyline),p=_29.rawNode.path.v.match(g.pathVmlRegExp);
do{
if(p.length<3||p[0]!="m"){
break;
}
var x=parseInt(p[0]),y=parseInt(p[1]);
if(isNaN(x)||isNaN(y)){
break;
}
_2a.points.push({x:x,y:y});
if(p.length<6||p[3]!="l"){
break;
}
for(var i=4;i<p.length;i+=2){
x=parseInt(p[i]);
y=parseInt(p[i+1]);
if(isNaN(x)||isNaN(y)){
break;
}
_2a.points.push({x:x,y:y});
}
}while(false);
};
var _10=function(_2b){
_2b.shape=_2.clone(g.defaultImage);
_2b.shape.src=_2b.rawNode.firstChild.src;
};
var _11=function(_2c){
var mm=_2c.rawNode.filters["DXImageTransform.Microsoft.Matrix"];
_2c.matrix=m.normalize({xx:mm.M11,xy:mm.M12,yx:mm.M21,yy:mm.M22,dx:mm.Dx,dy:mm.Dy});
};
var _d=function(_2d){
var _2e=_2d.shape=_2.clone(g.defaultText),r=_2d.rawNode,p=r.path.v.match(g.pathVmlRegExp);
do{
if(!p||p.length!=7){
break;
}
var c=r.childNodes,i=0;
for(;i<c.length&&c[i].tagName!="textpath";++i){
}
if(i>=c.length){
break;
}
var s=c[i].style;
_2e.text=c[i].string;
switch(s["v-text-align"]){
case "left":
_2e.x=parseInt(p[1]);
_2e.align="start";
break;
case "center":
_2e.x=(parseInt(p[1])+parseInt(p[4]))/2;
_2e.align="middle";
break;
case "right":
_2e.x=parseInt(p[4]);
_2e.align="end";
break;
}
_2e.y=parseInt(p[2]);
_2e.decoration=s["text-decoration"];
_2e.rotated=s["v-rotate-letters"].toLowerCase() in _5._bool;
_2e.kerning=s["v-text-kern"].toLowerCase() in _5._bool;
return;
}while(false);
_2d.shape=null;
};
var _e=function(_2f){
var _30=_2f.fontStyle=_2.clone(g.defaultFont),c=_2f.rawNode.childNodes,i=0;
for(;i<c.length&&c[i].tagName=="textpath";++i){
}
if(i>=c.length){
_2f.fontStyle=null;
return;
}
var s=c[i].style;
_30.style=s.fontstyle;
_30.variant=s.fontvariant;
_30.weight=s.fontweight;
_30.size=s.fontsize;
_30.family=s.fontfamily;
};
var _f=function(_31){
_14(_31);
var _32=_31.matrix,fs=_31.fontStyle;
if(_32&&fs){
_31.matrix=m.multiply(_32,{dy:g.normalizedLength(fs.size)*0.35});
}
};
var _c=function(_33){
var _34=_33.shape=_2.clone(g.defaultPath),p=_33.rawNode.path.v.match(g.pathVmlRegExp),t=[],_35=false,map=_3._pathVmlToSvgMap;
for(var i=0;i<p.length;++p){
var s=p[i];
if(s in map){
_35=false;
t.push(map[s]);
}else{
if(!_35){
var n=parseInt(s);
if(isNaN(n)){
_35=true;
}else{
t.push(n);
}
}
}
}
var l=t.length;
if(l>=4&&t[l-1]==""&&t[l-2]==0&&t[l-3]==0&&t[l-4]=="l"){
t.splice(l-4,4);
}
if(l){
_34.path=t.join(" ");
}
};
return _5;
});
