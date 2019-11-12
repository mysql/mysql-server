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
_1d&&_1d.parentNode.removeChild(_1d);
}
}
this.rawNode=null;
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
if(da in _b.dasharray){
da=_b.dasharray[da];
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
img.setAttribute("width",f.width.toFixed(8));
img.setAttribute("height",f.height.toFixed(8));
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
r.__gfxObject__=this.getUID();
},setShape:function(_2c){
this.shape=g.makeParameters(this.shape,_2c);
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
this.rawNode.parentNode.appendChild(_2f);
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
this.rawNode.__gfxObject__=this.getUID();
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
this.rawNode.setAttribute(i,this.shape[i]);
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
_3d.setAttribute(i,this.shape[i]);
}
}
_3d.setAttribute("preserveAspectRatio","none");
_12(_3d,_b.xmlns.xlink,"xlink:href",this.shape.src);
_3d.__gfxObject__=this.getUID();
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
}});
_b.Text.nodeType="text";
_b.Path=_5("dojox.gfx.svg.Path",[_b.Shape,_a.Path],{_updateWithSegment:function(_44){
this.inherited(arguments);
if(typeof (this.shape.path)=="string"){
this.rawNode.setAttribute("d",this.shape.path);
}
},setShape:function(_45){
this.inherited(arguments);
if(this.shape.path){
this.rawNode.setAttribute("d",this.shape.path);
}else{
this.rawNode.removeAttribute("d");
}
return this;
}});
_b.Path.nodeType="path";
_b.TextPath=_5("dojox.gfx.svg.TextPath",[_b.Shape,_a.TextPath],{_updateWithSegment:function(_46){
this.inherited(arguments);
this._setTextPath();
},setShape:function(_47){
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
var ref=r.firstChild.getAttributeNS(_b.xmlns.xlink,"href"),_48=ref&&_b.getRef(ref);
if(!_48){
var _49=this._getParentSurface();
if(_49){
var _4a=_49.defNode;
_48=_10(_b.xmlns.svg,"path");
var id=g._base._getUniqueId();
_48.setAttribute("id",id);
_4a.appendChild(_48);
_12(r.firstChild,_b.xmlns.xlink,"xlink:href","#"+id);
}
}
if(_48){
_48.setAttribute("d",this.shape.path);
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
var _4b=(function(){
var _4c=/WebKit\/(\d*)/.exec(_c);
return _4c?_4c[1]:0;
})()>534;
_b.Surface=_5("dojox.gfx.svg.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
gs.Container.clear.call(this,true);
this.defNode=null;
this.inherited(arguments);
},setDimensions:function(_4d,_4e){
if(!this.rawNode){
return this;
}
this.rawNode.setAttribute("width",_4d);
this.rawNode.setAttribute("height",_4e);
if(_4b){
this.rawNode.style.width=_4d;
this.rawNode.style.height=_4e;
}
return this;
},getDimensions:function(){
var t=this.rawNode?{width:g.normalizedLength(this.rawNode.getAttribute("width")),height:g.normalizedLength(this.rawNode.getAttribute("height"))}:null;
return t;
}});
_b.createSurface=function(_4f,_50,_51){
var s=new _b.Surface();
s.rawNode=_10(_b.xmlns.svg,"svg");
s.rawNode.setAttribute("overflow","hidden");
if(_50){
s.rawNode.setAttribute("width",_50);
}
if(_51){
s.rawNode.setAttribute("height",_51);
}
var _52=_10(_b.xmlns.svg,"defs");
s.rawNode.appendChild(_52);
s.defNode=_52;
s._parent=_4.byId(_4f);
s._parent.appendChild(s.rawNode);
return s;
};
var _53={_setFont:function(){
var f=this.fontStyle;
this.rawNode.setAttribute("font-style",f.style);
this.rawNode.setAttribute("font-variant",f.variant);
this.rawNode.setAttribute("font-weight",f.weight);
this.rawNode.setAttribute("font-size",f.size);
this.rawNode.setAttribute("font-family",f.family);
}};
var C=gs.Container,_54={openBatch:function(){
this.fragment=_18();
},closeBatch:function(){
if(this.fragment){
this.rawNode.appendChild(this.fragment);
delete this.fragment;
}
},add:function(_55){
if(this!=_55.getParent()){
if(this.fragment){
this.fragment.appendChild(_55.rawNode);
}else{
this.rawNode.appendChild(_55.rawNode);
}
C.add.apply(this,arguments);
_55.setClip(_55.clip);
}
return this;
},remove:function(_56,_57){
if(this==_56.getParent()){
if(this.rawNode==_56.rawNode.parentNode){
this.rawNode.removeChild(_56.rawNode);
}
if(this.fragment&&this.fragment==_56.rawNode.parentNode){
this.fragment.removeChild(_56.rawNode);
}
_56._removeClipNode();
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
var r=this.rawNode;
while(r.lastChild){
r.removeChild(r.lastChild);
}
var _58=this.defNode;
if(_58){
while(_58.lastChild){
_58.removeChild(_58.lastChild);
}
r.appendChild(_58);
}
return C.clear.apply(this,arguments);
},getBoundingBox:C.getBoundingBox,_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _59={createObject:function(_5a,_5b){
if(!this.rawNode){
return null;
}
var _5c=new _5a(),_5d=_10(_b.xmlns.svg,_5a.nodeType);
_5c.setRawNode(_5d);
_5c.setShape(_5b);
this.add(_5c);
return _5c;
}};
_1.extend(_b.Text,_53);
_1.extend(_b.TextPath,_53);
_1.extend(_b.Group,_54);
_1.extend(_b.Group,gs.Creator);
_1.extend(_b.Group,_59);
_1.extend(_b.Surface,_54);
_1.extend(_b.Surface,gs.Creator);
_1.extend(_b.Surface,_59);
_b.fixTarget=function(_5e,_5f){
if(!_5e.gfxTarget){
if(_d&&_5e.target.wholeText){
_5e.gfxTarget=gs.byId(_5e.target.parentElement.__gfxObject__);
}else{
_5e.gfxTarget=gs.byId(_5e.target.__gfxObject__);
}
}
return true;
};
if(_b.useSvgWeb){
_b.createSurface=function(_60,_61,_62){
var s=new _b.Surface();
if(!_61||!_62){
var pos=_7.position(_60);
_61=_61||pos.w;
_62=_62||pos.h;
}
_60=_4.byId(_60);
var id=_60.id?_60.id+"_svgweb":g._base._getUniqueId();
var _63=_10(_b.xmlns.svg,"svg");
_63.id=id;
_63.setAttribute("width",_61);
_63.setAttribute("height",_62);
svgweb.appendChild(_63,_60);
_63.addEventListener("SVGLoad",function(){
s.rawNode=this;
s.isLoaded=true;
var _64=_10(_b.xmlns.svg,"defs");
s.rawNode.appendChild(_64);
s.defNode=_64;
if(s.onLoad){
s.onLoad(s);
}
},false);
s.isLoaded=false;
return s;
};
_b.Surface.extend({destroy:function(){
var _65=this.rawNode;
svgweb.removeChild(_65,_65.parentNode);
}});
var _66={connect:function(_67,_68,_69){
if(_67.substring(0,2)==="on"){
_67=_67.substring(2);
}
if(arguments.length==2){
_69=_68;
}else{
_69=_1.hitch(_68,_69);
}
this.getEventSource().addEventListener(_67,_69,false);
return [this,_67,_69];
},disconnect:function(_6a){
this.getEventSource().removeEventListener(_6a[1],_6a[2],false);
delete _6a[0];
}};
_1.extend(_b.Shape,_66);
_1.extend(_b.Surface,_66);
}
return _b;
});
