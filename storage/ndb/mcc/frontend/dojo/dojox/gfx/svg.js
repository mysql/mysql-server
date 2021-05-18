//>>built
define("dojox/gfx/svg",["dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom","dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","dojo/dom-attr","dojo/_base/Color","./_base","./shape","./path"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,g,gs,_a){
var _b=g.svg={};
_b.useSvgWeb=(typeof window.svgweb!="undefined");
var _c=navigator.userAgent,_d=_2("ios"),_e=_2("android"),_f=_2("chrome")||(_e&&_e>=4)?"auto":"optimizeLegibility";
function _10(ns,_11){
if(_3.doc.createElementNS){
return _3.doc.createElementNS(ns,_11);
}else{
return _3.doc.createElement(_11);
}
};
function _12(_13,ns,_14,_15){
if(_13.setAttributeNS){
return _13.setAttributeNS(ns,_14,_15);
}else{
return _13.setAttribute(_14,_15);
}
};
function _16(_17){
if(_b.useSvgWeb){
return _3.doc.createTextNode(_17,true);
}else{
return _3.doc.createTextNode(_17);
}
};
function _18(){
if(_b.useSvgWeb){
return _3.doc.createDocumentFragment(true);
}else{
return _3.doc.createDocumentFragment();
}
};
_b.xmlns={xlink:"http://www.w3.org/1999/xlink",svg:"http://www.w3.org/2000/svg"};
_b.getRef=function(_19){
if(!_19||_19=="none"){
return null;
}
if(_19.match(/^url\(#.+\)$/)){
return _4.byId(_19.slice(5,-1));
}
if(_19.match(/^#dojoUnique\d+$/)){
return _4.byId(_19.slice(1));
}
return null;
};
_b.dasharray={solid:"none",shortdash:[4,1],shortdot:[1,1],shortdashdot:[4,1,1,1],shortdashdotdot:[4,1,1,1,1,1],dot:[1,3],dash:[4,3],longdash:[8,3],dashdot:[4,3,1,3],longdashdot:[8,3,1,3],longdashdotdot:[8,3,1,3,1,3]};
var _1a=0;
_b.Shape=_5("dojox.gfx.svg.Shape",gs.Shape,{destroy:function(){
if(this.fillStyle&&"type" in this.fillStyle){
var _1b=this.rawNode.getAttribute("fill"),ref=_b.getRef(_1b);
if(ref){
ref.parentNode.removeChild(ref);
}
}
if(this.clip){
var _1c=this.rawNode.getAttribute("clip-path");
if(_1c){
var _1d=_4.byId(_1c.match(/gfx_clip[\d]+/)[0]);
if(_1d){
_1d.parentNode.removeChild(_1d);
}
}
}
gs.Shape.prototype.destroy.apply(this,arguments);
},setFill:function(_1e){
if(!_1e){
this.fillStyle=null;
this.rawNode.setAttribute("fill","none");
this.rawNode.setAttribute("fill-opacity",0);
return this;
}
var f;
var _1f=function(x){
this.setAttribute(x,f[x].toFixed(8));
};
if(typeof (_1e)=="object"&&"type" in _1e){
switch(_1e.type){
case "linear":
f=g.makeParameters(g.defaultLinearGradient,_1e);
var _20=this._setFillObject(f,"linearGradient");
_6.forEach(["x1","y1","x2","y2"],_1f,_20);
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_1e);
var _21=this._setFillObject(f,"radialGradient");
_6.forEach(["cx","cy","r"],_1f,_21);
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_1e);
var _22=this._setFillObject(f,"pattern");
_6.forEach(["x","y","width","height"],_1f,_22);
break;
}
this.fillStyle=f;
return this;
}
f=g.normalizeColor(_1e);
this.fillStyle=f;
this.rawNode.setAttribute("fill",f.toCss());
this.rawNode.setAttribute("fill-opacity",f.a);
this.rawNode.setAttribute("fill-rule","evenodd");
return this;
},setStroke:function(_23){
var rn=this.rawNode;
if(!_23){
this.strokeStyle=null;
rn.setAttribute("stroke","none");
rn.setAttribute("stroke-opacity",0);
return this;
}
if(typeof _23=="string"||_1.isArray(_23)||_23 instanceof _9){
_23={color:_23};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_23);
s.color=g.normalizeColor(s.color);
if(s){
var w=s.width<0?0:s.width;
rn.setAttribute("stroke",s.color.toCss());
rn.setAttribute("stroke-opacity",s.color.a);
rn.setAttribute("stroke-width",w);
rn.setAttribute("stroke-linecap",s.cap);
if(typeof s.join=="number"){
rn.setAttribute("stroke-linejoin","miter");
rn.setAttribute("stroke-miterlimit",s.join);
}else{
rn.setAttribute("stroke-linejoin",s.join);
}
var da=s.style.toLowerCase();
if(da in _b.dasharray){
da=_b.dasharray[da];
}
if(da instanceof Array){
da=_1._toArray(da);
var i;
for(i=0;i<da.length;++i){
da[i]*=w;
}
if(s.cap!="butt"){
for(i=0;i<da.length;i+=2){
da[i]-=w;
if(da[i]<1){
da[i]=1;
}
}
for(i=1;i<da.length;i+=2){
da[i]+=w;
}
}
da=da.join(",");
}
rn.setAttribute("stroke-dasharray",da);
rn.setAttribute("dojoGfxStrokeStyle",s.style);
}
return this;
},_getParentSurface:function(){
var _24=this.parent;
for(;_24&&!(_24 instanceof g.Surface);_24=_24.parent){
}
return _24;
},_setFillObject:function(f,_25){
var _26=_b.xmlns.svg;
this.fillStyle=f;
var _27=this._getParentSurface(),_28=_27.defNode,_29=this.rawNode.getAttribute("fill"),ref=_b.getRef(_29);
if(ref){
_29=ref;
if(_29.tagName.toLowerCase()!=_25.toLowerCase()){
var id=_29.id;
_29.parentNode.removeChild(_29);
_29=_10(_26,_25);
_29.setAttribute("id",id);
_28.appendChild(_29);
}else{
while(_29.childNodes.length){
_29.removeChild(_29.lastChild);
}
}
}else{
_29=_10(_26,_25);
_29.setAttribute("id",g._base._getUniqueId());
_28.appendChild(_29);
}
if(_25=="pattern"){
_29.setAttribute("patternUnits","userSpaceOnUse");
var img=_10(_26,"image");
img.setAttribute("x",0);
img.setAttribute("y",0);
img.setAttribute("width",(f.width<0?0:f.width).toFixed(8));
img.setAttribute("height",(f.height<0?0:f.height).toFixed(8));
_12(img,_b.xmlns.xlink,"xlink:href",f.src);
_29.appendChild(img);
}else{
_29.setAttribute("gradientUnits","userSpaceOnUse");
for(var i=0;i<f.colors.length;++i){
var c=f.colors[i],t=_10(_26,"stop"),cc=c.color=g.normalizeColor(c.color);
t.setAttribute("offset",c.offset.toFixed(8));
t.setAttribute("stop-color",cc.toCss());
t.setAttribute("stop-opacity",cc.a);
_29.appendChild(t);
}
}
this.rawNode.setAttribute("fill","url(#"+_29.getAttribute("id")+")");
this.rawNode.removeAttribute("fill-opacity");
this.rawNode.setAttribute("fill-rule","evenodd");
return _29;
},_applyTransform:function(){
var _2a=this.matrix;
if(_2a){
var tm=this.matrix;
this.rawNode.setAttribute("transform","matrix("+tm.xx.toFixed(8)+","+tm.yx.toFixed(8)+","+tm.xy.toFixed(8)+","+tm.yy.toFixed(8)+","+tm.dx.toFixed(8)+","+tm.dy.toFixed(8)+")");
}else{
this.rawNode.removeAttribute("transform");
}
return this;
},setRawNode:function(_2b){
var r=this.rawNode=_2b;
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
r.__gfxObject__=this;
},setShape:function(_2c){
this.shape=g.makeParameters(this.shape,_2c);
for(var i in this.shape){
if(i!="type"){
var v=this.shape[i];
if(i==="width"||i==="height"){
v=v<0?0:v;
}
this.rawNode.setAttribute(i,v);
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
},setClip:function(_2d){
this.inherited(arguments);
var _2e=_2d?"width" in _2d?"rect":"cx" in _2d?"ellipse":"points" in _2d?"polyline":"d" in _2d?"path":null:null;
if(_2d&&!_2e){
return this;
}
if(_2e==="polyline"){
_2d=_1.clone(_2d);
_2d.points=_2d.points.join(",");
}
var _2f,_30,_31=_8.get(this.rawNode,"clip-path");
if(_31){
_2f=_4.byId(_31.match(/gfx_clip[\d]+/)[0]);
if(_2f){
_2f.removeChild(_2f.childNodes[0]);
}
}
if(_2d){
if(_2f){
_30=_10(_b.xmlns.svg,_2e);
_2f.appendChild(_30);
}else{
var _32=++_1a;
var _33="gfx_clip"+_32;
var _34="url(#"+_33+")";
this.rawNode.setAttribute("clip-path",_34);
_2f=_10(_b.xmlns.svg,"clipPath");
_30=_10(_b.xmlns.svg,_2e);
_2f.appendChild(_30);
this.rawNode.parentNode.insertBefore(_2f,this.rawNode);
_8.set(_2f,"id",_33);
}
_8.set(_30,_2d);
}else{
this.rawNode.removeAttribute("clip-path");
if(_2f){
_2f.parentNode.removeChild(_2f);
}
}
return this;
},_removeClipNode:function(){
var _35,_36=_8.get(this.rawNode,"clip-path");
if(_36){
_35=_4.byId(_36.match(/gfx_clip[\d]+/)[0]);
if(_35){
_35.parentNode.removeChild(_35);
}
}
return _35;
}});
_b.Group=_5("dojox.gfx.svg.Group",_b.Shape,{constructor:function(){
gs.Container._init.call(this);
},setRawNode:function(_37){
this.rawNode=_37;
this.rawNode.__gfxObject__=this;
},destroy:function(){
this.clear(true);
_b.Shape.prototype.destroy.apply(this,arguments);
}});
_b.Group.nodeType="g";
_b.Rect=_5("dojox.gfx.svg.Rect",[_b.Shape,gs.Rect],{setShape:function(_38){
this.shape=g.makeParameters(this.shape,_38);
this.bbox=null;
for(var i in this.shape){
if(i!="type"&&i!="r"){
var v=this.shape[i];
if(i==="width"||i==="height"){
v=v<0?0:v;
}
this.rawNode.setAttribute(i,v);
}
}
if(this.shape.r!=null){
this.rawNode.setAttribute("ry",this.shape.r);
this.rawNode.setAttribute("rx",this.shape.r);
}
return this;
}});
_b.Rect.nodeType="rect";
_b.Ellipse=_5("dojox.gfx.svg.Ellipse",[_b.Shape,gs.Ellipse],{});
_b.Ellipse.nodeType="ellipse";
_b.Circle=_5("dojox.gfx.svg.Circle",[_b.Shape,gs.Circle],{});
_b.Circle.nodeType="circle";
_b.Line=_5("dojox.gfx.svg.Line",[_b.Shape,gs.Line],{});
_b.Line.nodeType="line";
_b.Polyline=_5("dojox.gfx.svg.Polyline",[_b.Shape,gs.Polyline],{setShape:function(_39,_3a){
if(_39&&_39 instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_39});
if(_3a&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_39);
}
this.bbox=null;
this._normalizePoints();
var _3b=[],p=this.shape.points;
for(var i=0;i<p.length;++i){
_3b.push(p[i].x.toFixed(8),p[i].y.toFixed(8));
}
this.rawNode.setAttribute("points",_3b.join(" "));
return this;
}});
_b.Polyline.nodeType="polyline";
_b.Image=_5("dojox.gfx.svg.Image",[_b.Shape,gs.Image],{setShape:function(_3c){
this.shape=g.makeParameters(this.shape,_3c);
this.bbox=null;
var _3d=this.rawNode;
for(var i in this.shape){
if(i!="type"&&i!="src"){
var v=this.shape[i];
if(i==="width"||i==="height"){
v=v<0?0:v;
}
_3d.setAttribute(i,v);
}
}
_3d.setAttribute("preserveAspectRatio","none");
_12(_3d,_b.xmlns.xlink,"xlink:href",this.shape.src);
_3d.__gfxObject__=this;
return this;
}});
_b.Image.nodeType="image";
_b.Text=_5("dojox.gfx.svg.Text",[_b.Shape,gs.Text],{setShape:function(_3e){
this.shape=g.makeParameters(this.shape,_3e);
this.bbox=null;
var r=this.rawNode,s=this.shape;
r.setAttribute("x",s.x);
r.setAttribute("y",s.y);
r.setAttribute("text-anchor",s.align);
r.setAttribute("text-decoration",s.decoration);
r.setAttribute("rotate",s.rotated?90:0);
r.setAttribute("kerning",s.kerning?"auto":0);
r.setAttribute("text-rendering",_f);
if(r.firstChild){
r.firstChild.nodeValue=s.text;
}else{
r.appendChild(_16(s.text));
}
return this;
},getTextWidth:function(){
var _3f=this.rawNode,_40=_3f.parentNode,_41=_3f.cloneNode(true);
_41.style.visibility="hidden";
var _42=0,_43=_41.firstChild.nodeValue;
_40.appendChild(_41);
if(_43!=""){
while(!_42){
if(_41.getBBox){
_42=parseInt(_41.getBBox().width);
}else{
_42=68;
}
}
}
_40.removeChild(_41);
return _42;
},getBoundingBox:function(){
var s=this.getShape(),_44=null;
if(s.text){
try{
_44=this.rawNode.getBBox();
}
catch(e){
_44={x:0,y:0,width:0,height:0};
}
}
return _44;
}});
_b.Text.nodeType="text";
_b.Path=_5("dojox.gfx.svg.Path",[_b.Shape,_a.Path],{_updateWithSegment:function(_45){
this.inherited(arguments);
if(typeof (this.shape.path)=="string"){
this.rawNode.setAttribute("d",this.shape.path);
}
},setShape:function(_46){
this.inherited(arguments);
if(this.shape.path){
this.rawNode.setAttribute("d",this.shape.path);
}else{
this.rawNode.removeAttribute("d");
}
return this;
}});
_b.Path.nodeType="path";
_b.TextPath=_5("dojox.gfx.svg.TextPath",[_b.Shape,_a.TextPath],{_updateWithSegment:function(_47){
this.inherited(arguments);
this._setTextPath();
},setShape:function(_48){
this.inherited(arguments);
this._setTextPath();
return this;
},_setTextPath:function(){
if(typeof this.shape.path!="string"){
return;
}
var r=this.rawNode;
if(!r.firstChild){
var tp=_10(_b.xmlns.svg,"textPath"),tx=_16("");
tp.appendChild(tx);
r.appendChild(tp);
}
var ref=r.firstChild.getAttributeNS(_b.xmlns.xlink,"href"),_49=ref&&_b.getRef(ref);
if(!_49){
var _4a=this._getParentSurface();
if(_4a){
var _4b=_4a.defNode;
_49=_10(_b.xmlns.svg,"path");
var id=g._base._getUniqueId();
_49.setAttribute("id",id);
_4b.appendChild(_49);
_12(r.firstChild,_b.xmlns.xlink,"xlink:href","#"+id);
}
}
if(_49){
_49.setAttribute("d",this.shape.path);
}
},_setText:function(){
var r=this.rawNode;
if(!r.firstChild){
var tp=_10(_b.xmlns.svg,"textPath"),tx=_16("");
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
_b.TextPath.nodeType="text";
var _4c=(function(){
var _4d=/WebKit\/(\d*)/.exec(_c);
return _4d?_4d[1]:0;
})()>534;
_b.Surface=_5("dojox.gfx.svg.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
gs.Container.clear.call(this,true);
this.defNode=null;
this.inherited(arguments);
},setDimensions:function(_4e,_4f){
if(!this.rawNode){
return this;
}
var w=_4e<0?0:_4e,h=_4f<0?0:_4f;
this.rawNode.setAttribute("width",w);
this.rawNode.setAttribute("height",h);
if(_4c){
this.rawNode.style.width=w;
this.rawNode.style.height=h;
}
return this;
},getDimensions:function(){
var t=this.rawNode?{width:g.normalizedLength(this.rawNode.getAttribute("width")),height:g.normalizedLength(this.rawNode.getAttribute("height"))}:null;
return t;
}});
_b.createSurface=function(_50,_51,_52){
var s=new _b.Surface();
s.rawNode=_10(_b.xmlns.svg,"svg");
s.rawNode.setAttribute("overflow","hidden");
if(_51){
s.rawNode.setAttribute("width",_51<0?0:_51);
}
if(_52){
s.rawNode.setAttribute("height",_52<0?0:_52);
}
var _53=_10(_b.xmlns.svg,"defs");
s.rawNode.appendChild(_53);
s.defNode=_53;
s._parent=_4.byId(_50);
s._parent.appendChild(s.rawNode);
g._base._fixMsTouchAction(s);
return s;
};
var _54={_setFont:function(){
var f=this.fontStyle;
this.rawNode.setAttribute("font-style",f.style);
this.rawNode.setAttribute("font-variant",f.variant);
this.rawNode.setAttribute("font-weight",f.weight);
this.rawNode.setAttribute("font-size",f.size);
this.rawNode.setAttribute("font-family",f.family);
}};
var C=gs.Container;
var _55=_b.Container={openBatch:function(){
if(!this._batch){
this.fragment=_18();
}
++this._batch;
return this;
},closeBatch:function(){
this._batch=this._batch>0?--this._batch:0;
if(this.fragment&&!this._batch){
this.rawNode.appendChild(this.fragment);
delete this.fragment;
}
return this;
},add:function(_56){
if(this!=_56.getParent()){
if(this.fragment){
this.fragment.appendChild(_56.rawNode);
}else{
this.rawNode.appendChild(_56.rawNode);
}
C.add.apply(this,arguments);
_56.setClip(_56.clip);
}
return this;
},remove:function(_57,_58){
if(this==_57.getParent()){
if(this.rawNode==_57.rawNode.parentNode){
this.rawNode.removeChild(_57.rawNode);
}
if(this.fragment&&this.fragment==_57.rawNode.parentNode){
this.fragment.removeChild(_57.rawNode);
}
_57._removeClipNode();
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
var r=this.rawNode;
while(r.lastChild){
r.removeChild(r.lastChild);
}
var _59=this.defNode;
if(_59){
while(_59.lastChild){
_59.removeChild(_59.lastChild);
}
r.appendChild(_59);
}
return C.clear.apply(this,arguments);
},getBoundingBox:C.getBoundingBox,_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _5a=_b.Creator={createObject:function(_5b,_5c){
if(!this.rawNode){
return null;
}
var _5d=new _5b(),_5e=_10(_b.xmlns.svg,_5b.nodeType);
_5d.setRawNode(_5e);
_5d.setShape(_5c);
this.add(_5d);
return _5d;
}};
_1.extend(_b.Text,_54);
_1.extend(_b.TextPath,_54);
_1.extend(_b.Group,_55);
_1.extend(_b.Group,gs.Creator);
_1.extend(_b.Group,_5a);
_1.extend(_b.Surface,_55);
_1.extend(_b.Surface,gs.Creator);
_1.extend(_b.Surface,_5a);
_b.fixTarget=function(_5f,_60){
if(!_5f.gfxTarget){
if(_d&&_5f.target.wholeText){
_5f.gfxTarget=_5f.target.parentElement.__gfxObject__;
}else{
_5f.gfxTarget=_5f.target.__gfxObject__;
}
}
return true;
};
if(_b.useSvgWeb){
_b.createSurface=function(_61,_62,_63){
var s=new _b.Surface();
_62=_62<0?0:_62;
_63=_63<0?0:_63;
if(!_62||!_63){
var pos=_7.position(_61);
_62=_62||pos.w;
_63=_63||pos.h;
}
_61=_4.byId(_61);
var id=_61.id?_61.id+"_svgweb":g._base._getUniqueId();
var _64=_10(_b.xmlns.svg,"svg");
_64.id=id;
_64.setAttribute("width",_62);
_64.setAttribute("height",_63);
svgweb.appendChild(_64,_61);
_64.addEventListener("SVGLoad",function(){
s.rawNode=this;
s.isLoaded=true;
var _65=_10(_b.xmlns.svg,"defs");
s.rawNode.appendChild(_65);
s.defNode=_65;
if(s.onLoad){
s.onLoad(s);
}
},false);
s.isLoaded=false;
return s;
};
_b.Surface.extend({destroy:function(){
var _66=this.rawNode;
svgweb.removeChild(_66,_66.parentNode);
}});
var _67={connect:function(_68,_69,_6a){
if(_68.substring(0,2)==="on"){
_68=_68.substring(2);
}
if(arguments.length==2){
_6a=_69;
}else{
_6a=_1.hitch(_69,_6a);
}
this.getEventSource().addEventListener(_68,_6a,false);
return [this,_68,_6a];
},disconnect:function(_6b){
this.getEventSource().removeEventListener(_6b[1],_6b[2],false);
delete _6b[0];
}};
_1.extend(_b.Shape,_67);
_1.extend(_b.Surface,_67);
}
return _b;
});
