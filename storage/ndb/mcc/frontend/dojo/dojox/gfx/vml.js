//>>built
define("dojox/gfx/vml",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/Color","dojo/_base/sniff","dojo/_base/config","dojo/dom","dojo/dom-geometry","dojo/_base/window","./_base","./shape","./path","./arc","./gradient","./matrix"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,g,gs,_a,_b,_c,m){
var _d=g.vml={};
_d.xmlns="urn:schemas-microsoft-com:vml";
document.namespaces.add("v",_d.xmlns);
var _e=["*","group","roundrect","oval","shape","rect","imagedata","path","textpath","text"],i=0,l=1,s=document.createStyleSheet();
if(_5("ie")>=8){
i=1;
l=_e.length;
}
for(;i<l;++i){
s.addRule("v\\:"+_e[i],"behavior:url(#default#VML); display:inline-block");
}
_d.text_alignment={start:"left",middle:"center",end:"right"};
_d._parseFloat=function(_f){
return _f.match(/^\d+f$/i)?parseInt(_f)/65536:parseFloat(_f);
};
_d._bool={"t":1,"true":1};
_2("dojox.gfx.vml.Shape",gs.Shape,{setFill:function(_10){
if(!_10){
this.fillStyle=null;
this.rawNode.filled="f";
return this;
}
var i,f,fo,a,s;
if(typeof _10=="object"&&"type" in _10){
switch(_10.type){
case "linear":
var _11=this._getRealMatrix(),_12=this.getBoundingBox(),_13=this._getRealBBox?this._getRealBBox():this.getTransformedBoundingBox();
s=[];
if(this.fillStyle!==_10){
this.fillStyle=g.makeParameters(g.defaultLinearGradient,_10);
}
f=g.gradient.project(_11,this.fillStyle,{x:_12.x,y:_12.y},{x:_12.x+_12.width,y:_12.y+_12.height},_13[0],_13[2]);
a=f.colors;
if(a[0].offset.toFixed(5)!="0.00000"){
s.push("0 "+g.normalizeColor(a[0].color).toHex());
}
for(i=0;i<a.length;++i){
s.push(a[i].offset.toFixed(5)+" "+g.normalizeColor(a[i].color).toHex());
}
i=a.length-1;
if(a[i].offset.toFixed(5)!="1.00000"){
s.push("1 "+g.normalizeColor(a[i].color).toHex());
}
fo=this.rawNode.fill;
fo.colors.value=s.join(";");
fo.method="sigma";
fo.type="gradient";
fo.angle=(270-m._radToDeg(f.angle))%360;
fo.on=true;
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_10);
this.fillStyle=f;
var l=parseFloat(this.rawNode.style.left),t=parseFloat(this.rawNode.style.top),w=parseFloat(this.rawNode.style.width),h=parseFloat(this.rawNode.style.height),c=isNaN(w)?1:2*f.r/w;
a=[];
if(f.colors[0].offset>0){
a.push({offset:1,color:g.normalizeColor(f.colors[0].color)});
}
_3.forEach(f.colors,function(v,i){
a.push({offset:1-v.offset*c,color:g.normalizeColor(v.color)});
});
i=a.length-1;
while(i>=0&&a[i].offset<0){
--i;
}
if(i<a.length-1){
var q=a[i],p=a[i+1];
p.color=_4.blendColors(q.color,p.color,q.offset/(q.offset-p.offset));
p.offset=0;
while(a.length-i>2){
a.pop();
}
}
i=a.length-1,s=[];
if(a[i].offset>0){
s.push("0 "+a[i].color.toHex());
}
for(;i>=0;--i){
s.push(a[i].offset.toFixed(5)+" "+a[i].color.toHex());
}
fo=this.rawNode.fill;
fo.colors.value=s.join(";");
fo.method="sigma";
fo.type="gradientradial";
if(isNaN(w)||isNaN(h)||isNaN(l)||isNaN(t)){
fo.focusposition="0.5 0.5";
}else{
fo.focusposition=((f.cx-l)/w).toFixed(5)+" "+((f.cy-t)/h).toFixed(5);
}
fo.focussize="0 0";
fo.on=true;
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_10);
this.fillStyle=f;
fo=this.rawNode.fill;
fo.type="tile";
fo.src=f.src;
if(f.width&&f.height){
fo.size.x=g.px2pt(f.width);
fo.size.y=g.px2pt(f.height);
}
fo.alignShape="f";
fo.position.x=0;
fo.position.y=0;
fo.origin.x=f.width?f.x/f.width:0;
fo.origin.y=f.height?f.y/f.height:0;
fo.on=true;
break;
}
this.rawNode.fill.opacity=1;
return this;
}
this.fillStyle=g.normalizeColor(_10);
fo=this.rawNode.fill;
if(!fo){
fo=this.rawNode.ownerDocument.createElement("v:fill");
}
fo.method="any";
fo.type="solid";
fo.opacity=this.fillStyle.a;
var _14=this.rawNode.filters["DXImageTransform.Microsoft.Alpha"];
if(_14){
_14.opacity=Math.round(this.fillStyle.a*100);
}
this.rawNode.fillcolor=this.fillStyle.toHex();
this.rawNode.filled=true;
return this;
},setStroke:function(_15){
if(!_15){
this.strokeStyle=null;
this.rawNode.stroked="f";
return this;
}
if(typeof _15=="string"||_1.isArray(_15)||_15 instanceof _4){
_15={color:_15};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_15);
s.color=g.normalizeColor(s.color);
var rn=this.rawNode;
rn.stroked=true;
rn.strokecolor=s.color.toCss();
rn.strokeweight=s.width+"px";
if(rn.stroke){
rn.stroke.opacity=s.color.a;
rn.stroke.endcap=this._translate(this._capMap,s.cap);
if(typeof s.join=="number"){
rn.stroke.joinstyle="miter";
rn.stroke.miterlimit=s.join;
}else{
rn.stroke.joinstyle=s.join;
}
rn.stroke.dashstyle=s.style=="none"?"Solid":s.style;
}
return this;
},_capMap:{butt:"flat"},_capMapReversed:{flat:"butt"},_translate:function(_16,_17){
return (_17 in _16)?_16[_17]:_17;
},_applyTransform:function(){
var _18=this._getRealMatrix();
if(_18){
var _19=this.rawNode.skew;
if(typeof _19=="undefined"){
for(var i=0;i<this.rawNode.childNodes.length;++i){
if(this.rawNode.childNodes[i].tagName=="skew"){
_19=this.rawNode.childNodes[i];
break;
}
}
}
if(_19){
_19.on="f";
var mt=_18.xx.toFixed(8)+" "+_18.xy.toFixed(8)+" "+_18.yx.toFixed(8)+" "+_18.yy.toFixed(8)+" 0 0",_1a=Math.floor(_18.dx).toFixed()+"px "+Math.floor(_18.dy).toFixed()+"px",s=this.rawNode.style,l=parseFloat(s.left),t=parseFloat(s.top),w=parseFloat(s.width),h=parseFloat(s.height);
if(isNaN(l)){
l=0;
}
if(isNaN(t)){
t=0;
}
if(isNaN(w)||!w){
w=1;
}
if(isNaN(h)||!h){
h=1;
}
var _1b=(-l/w-0.5).toFixed(8)+" "+(-t/h-0.5).toFixed(8);
_19.matrix=mt;
_19.origin=_1b;
_19.offset=_1a;
_19.on=true;
}
}
if(this.fillStyle&&this.fillStyle.type=="linear"){
this.setFill(this.fillStyle);
}
return this;
},_setDimensions:function(_1c,_1d){
return this;
},setRawNode:function(_1e){
_1e.stroked="f";
_1e.filled="f";
this.rawNode=_1e;
this.rawNode.__gfxObject__=this.getUID();
},_moveToFront:function(){
this.rawNode.parentNode.appendChild(this.rawNode);
return this;
},_moveToBack:function(){
var r=this.rawNode,p=r.parentNode,n=p.firstChild;
p.insertBefore(r,n);
if(n.tagName=="rect"){
n.swapNode(r);
}
return this;
},_getRealMatrix:function(){
return this.parentMatrix?new m.Matrix2D([this.parentMatrix,this.matrix]):this.matrix;
}});
_2("dojox.gfx.vml.Group",_d.Shape,{constructor:function(){
gs.Container._init.call(this);
},_applyTransform:function(){
var _1f=this._getRealMatrix();
for(var i=0;i<this.children.length;++i){
this.children[i]._updateParentMatrix(_1f);
}
return this;
},_setDimensions:function(_20,_21){
var r=this.rawNode,rs=r.style,bs=this.bgNode.style;
rs.width=_20;
rs.height=_21;
r.coordsize=_20+" "+_21;
bs.width=_20;
bs.height=_21;
for(var i=0;i<this.children.length;++i){
this.children[i]._setDimensions(_20,_21);
}
return this;
}});
_d.Group.nodeType="group";
_2("dojox.gfx.vml.Rect",[_d.Shape,gs.Rect],{setShape:function(_22){
var _23=this.shape=g.makeParameters(this.shape,_22);
this.bbox=null;
var r=Math.min(1,(_23.r/Math.min(parseFloat(_23.width),parseFloat(_23.height)))).toFixed(8);
var _24=this.rawNode.parentNode,_25=null;
if(_24){
if(_24.lastChild!==this.rawNode){
for(var i=0;i<_24.childNodes.length;++i){
if(_24.childNodes[i]===this.rawNode){
_25=_24.childNodes[i+1];
break;
}
}
}
_24.removeChild(this.rawNode);
}
if(_5("ie")>7){
var _26=this.rawNode.ownerDocument.createElement("v:roundrect");
_26.arcsize=r;
_26.style.display="inline-block";
this.rawNode=_26;
this.rawNode.__gfxObject__=this.getUID();
}else{
this.rawNode.arcsize=r;
}
if(_24){
if(_25){
_24.insertBefore(this.rawNode,_25);
}else{
_24.appendChild(this.rawNode);
}
}
var _27=this.rawNode.style;
_27.left=_23.x.toFixed();
_27.top=_23.y.toFixed();
_27.width=(typeof _23.width=="string"&&_23.width.indexOf("%")>=0)?_23.width:Math.max(_23.width.toFixed(),0);
_27.height=(typeof _23.height=="string"&&_23.height.indexOf("%")>=0)?_23.height:Math.max(_23.height.toFixed(),0);
return this.setTransform(this.matrix).setFill(this.fillStyle).setStroke(this.strokeStyle);
}});
_d.Rect.nodeType="roundrect";
_2("dojox.gfx.vml.Ellipse",[_d.Shape,gs.Ellipse],{setShape:function(_28){
var _29=this.shape=g.makeParameters(this.shape,_28);
this.bbox=null;
var _2a=this.rawNode.style;
_2a.left=(_29.cx-_29.rx).toFixed();
_2a.top=(_29.cy-_29.ry).toFixed();
_2a.width=(_29.rx*2).toFixed();
_2a.height=(_29.ry*2).toFixed();
return this.setTransform(this.matrix);
}});
_d.Ellipse.nodeType="oval";
_2("dojox.gfx.vml.Circle",[_d.Shape,gs.Circle],{setShape:function(_2b){
var _2c=this.shape=g.makeParameters(this.shape,_2b);
this.bbox=null;
var _2d=this.rawNode.style;
_2d.left=(_2c.cx-_2c.r).toFixed();
_2d.top=(_2c.cy-_2c.r).toFixed();
_2d.width=(_2c.r*2).toFixed();
_2d.height=(_2c.r*2).toFixed();
return this;
}});
_d.Circle.nodeType="oval";
_2("dojox.gfx.vml.Line",[_d.Shape,gs.Line],{constructor:function(_2e){
if(_2e){
_2e.setAttribute("dojoGfxType","line");
}
},setShape:function(_2f){
var _30=this.shape=g.makeParameters(this.shape,_2f);
this.bbox=null;
this.rawNode.path.v="m"+_30.x1.toFixed()+" "+_30.y1.toFixed()+"l"+_30.x2.toFixed()+" "+_30.y2.toFixed()+"e";
return this.setTransform(this.matrix);
}});
_d.Line.nodeType="shape";
_2("dojox.gfx.vml.Polyline",[_d.Shape,gs.Polyline],{constructor:function(_31){
if(_31){
_31.setAttribute("dojoGfxType","polyline");
}
},setShape:function(_32,_33){
if(_32&&_32 instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_32});
if(_33&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_32);
}
this.bbox=null;
this._normalizePoints();
var _34=[],p=this.shape.points;
if(p.length>0){
_34.push("m");
_34.push(p[0].x.toFixed(),p[0].y.toFixed());
if(p.length>1){
_34.push("l");
for(var i=1;i<p.length;++i){
_34.push(p[i].x.toFixed(),p[i].y.toFixed());
}
}
}
_34.push("e");
this.rawNode.path.v=_34.join(" ");
return this.setTransform(this.matrix);
}});
_d.Polyline.nodeType="shape";
_2("dojox.gfx.vml.Image",[_d.Shape,gs.Image],{setShape:function(_35){
var _36=this.shape=g.makeParameters(this.shape,_35);
this.bbox=null;
this.rawNode.firstChild.src=_36.src;
return this.setTransform(this.matrix);
},_applyTransform:function(){
var _37=this._getRealMatrix(),_38=this.rawNode,s=_38.style,_39=this.shape;
if(_37){
_37=m.multiply(_37,{dx:_39.x,dy:_39.y});
}else{
_37=m.normalize({dx:_39.x,dy:_39.y});
}
if(_37.xy==0&&_37.yx==0&&_37.xx>0&&_37.yy>0){
s.filter="";
s.width=Math.floor(_37.xx*_39.width);
s.height=Math.floor(_37.yy*_39.height);
s.left=Math.floor(_37.dx);
s.top=Math.floor(_37.dy);
}else{
var ps=_38.parentNode.style;
s.left="0px";
s.top="0px";
s.width=ps.width;
s.height=ps.height;
_37=m.multiply(_37,{xx:_39.width/parseInt(s.width),yy:_39.height/parseInt(s.height)});
var f=_38.filters["DXImageTransform.Microsoft.Matrix"];
if(f){
f.M11=_37.xx;
f.M12=_37.xy;
f.M21=_37.yx;
f.M22=_37.yy;
f.Dx=_37.dx;
f.Dy=_37.dy;
}else{
s.filter="progid:DXImageTransform.Microsoft.Matrix(M11="+_37.xx+", M12="+_37.xy+", M21="+_37.yx+", M22="+_37.yy+", Dx="+_37.dx+", Dy="+_37.dy+")";
}
}
return this;
},_setDimensions:function(_3a,_3b){
var r=this.rawNode,f=r.filters["DXImageTransform.Microsoft.Matrix"];
if(f){
var s=r.style;
s.width=_3a;
s.height=_3b;
return this._applyTransform();
}
return this;
}});
_d.Image.nodeType="rect";
_2("dojox.gfx.vml.Text",[_d.Shape,gs.Text],{constructor:function(_3c){
if(_3c){
_3c.setAttribute("dojoGfxType","text");
}
this.fontStyle=null;
},_alignment:{start:"left",middle:"center",end:"right"},setShape:function(_3d){
this.shape=g.makeParameters(this.shape,_3d);
this.bbox=null;
var r=this.rawNode,s=this.shape,x=s.x,y=s.y.toFixed(),_3e;
switch(s.align){
case "middle":
x-=5;
break;
case "end":
x-=10;
break;
}
_3e="m"+x.toFixed()+","+y+"l"+(x+10).toFixed()+","+y+"e";
var p=null,t=null,c=r.childNodes;
for(var i=0;i<c.length;++i){
var tag=c[i].tagName;
if(tag=="path"){
p=c[i];
if(t){
break;
}
}else{
if(tag=="textpath"){
t=c[i];
if(p){
break;
}
}
}
}
if(!p){
p=r.ownerDocument.createElement("v:path");
r.appendChild(p);
}
if(!t){
t=r.ownerDocument.createElement("v:textpath");
r.appendChild(t);
}
p.v=_3e;
p.textPathOk=true;
t.on=true;
var a=_d.text_alignment[s.align];
t.style["v-text-align"]=a?a:"left";
t.style["text-decoration"]=s.decoration;
t.style["v-rotate-letters"]=s.rotated;
t.style["v-text-kern"]=s.kerning;
t.string=s.text;
return this.setTransform(this.matrix);
},_setFont:function(){
var f=this.fontStyle,c=this.rawNode.childNodes;
for(var i=0;i<c.length;++i){
if(c[i].tagName=="textpath"){
c[i].style.font=g.makeFontString(f);
break;
}
}
this.setTransform(this.matrix);
},_getRealMatrix:function(){
var _3f=this.inherited(arguments);
if(_3f){
_3f=m.multiply(_3f,{dy:-g.normalizedLength(this.fontStyle?this.fontStyle.size:"10pt")*0.35});
}
return _3f;
},getTextWidth:function(){
var _40=this.rawNode,_41=_40.style.display;
_40.style.display="inline";
var _42=g.pt2px(parseFloat(_40.currentStyle.width));
_40.style.display=_41;
return _42;
}});
_d.Text.nodeType="shape";
_2("dojox.gfx.vml.Path",[_d.Shape,_a.Path],{constructor:function(_43){
if(_43&&!_43.getAttribute("dojoGfxType")){
_43.setAttribute("dojoGfxType","path");
}
this.vmlPath="";
this.lastControl={};
},_updateWithSegment:function(_44){
var _45=_1.clone(this.last);
this.inherited(arguments);
if(arguments.length>1){
return;
}
var _46=this[this.renderers[_44.action]](_44,_45);
if(typeof this.vmlPath=="string"){
this.vmlPath+=_46.join("");
this.rawNode.path.v=this.vmlPath+" r0,0 e";
}else{
Array.prototype.push.apply(this.vmlPath,_46);
}
},setShape:function(_47){
this.vmlPath=[];
this.lastControl.type="";
this.inherited(arguments);
this.vmlPath=this.vmlPath.join("");
this.rawNode.path.v=this.vmlPath+" r0,0 e";
return this;
},_pathVmlToSvgMap:{m:"M",l:"L",t:"m",r:"l",c:"C",v:"c",qb:"Q",x:"z",e:""},renderers:{M:"_moveToA",m:"_moveToR",L:"_lineToA",l:"_lineToR",H:"_hLineToA",h:"_hLineToR",V:"_vLineToA",v:"_vLineToR",C:"_curveToA",c:"_curveToR",S:"_smoothCurveToA",s:"_smoothCurveToR",Q:"_qCurveToA",q:"_qCurveToR",T:"_qSmoothCurveToA",t:"_qSmoothCurveToR",A:"_arcTo",a:"_arcTo",Z:"_closePath",z:"_closePath"},_addArgs:function(_48,_49,_4a,_4b){
var n=_49 instanceof Array?_49:_49.args;
for(var i=_4a;i<_4b;++i){
_48.push(" ",n[i].toFixed());
}
},_adjustRelCrd:function(_4c,_4d,_4e){
var n=_4d instanceof Array?_4d:_4d.args,l=n.length,_4f=new Array(l),i=0,x=_4c.x,y=_4c.y;
if(typeof x!="number"){
_4f[0]=x=n[0];
_4f[1]=y=n[1];
i=2;
}
if(typeof _4e=="number"&&_4e!=2){
var j=_4e;
while(j<=l){
for(;i<j;i+=2){
_4f[i]=x+n[i];
_4f[i+1]=y+n[i+1];
}
x=_4f[j-2];
y=_4f[j-1];
j+=_4e;
}
}else{
for(;i<l;i+=2){
_4f[i]=(x+=n[i]);
_4f[i+1]=(y+=n[i+1]);
}
}
return _4f;
},_adjustRelPos:function(_50,_51){
var n=_51 instanceof Array?_51:_51.args,l=n.length,_52=new Array(l);
for(var i=0;i<l;++i){
_52[i]=(_50+=n[i]);
}
return _52;
},_moveToA:function(_53){
var p=[" m"],n=_53 instanceof Array?_53:_53.args,l=n.length;
this._addArgs(p,n,0,2);
if(l>2){
p.push(" l");
this._addArgs(p,n,2,l);
}
this.lastControl.type="";
return p;
},_moveToR:function(_54,_55){
return this._moveToA(this._adjustRelCrd(_55,_54));
},_lineToA:function(_56){
var p=[" l"],n=_56 instanceof Array?_56:_56.args;
this._addArgs(p,n,0,n.length);
this.lastControl.type="";
return p;
},_lineToR:function(_57,_58){
return this._lineToA(this._adjustRelCrd(_58,_57));
},_hLineToA:function(_59,_5a){
var p=[" l"],y=" "+_5a.y.toFixed(),n=_59 instanceof Array?_59:_59.args,l=n.length;
for(var i=0;i<l;++i){
p.push(" ",n[i].toFixed(),y);
}
this.lastControl.type="";
return p;
},_hLineToR:function(_5b,_5c){
return this._hLineToA(this._adjustRelPos(_5c.x,_5b),_5c);
},_vLineToA:function(_5d,_5e){
var p=[" l"],x=" "+_5e.x.toFixed(),n=_5d instanceof Array?_5d:_5d.args,l=n.length;
for(var i=0;i<l;++i){
p.push(x," ",n[i].toFixed());
}
this.lastControl.type="";
return p;
},_vLineToR:function(_5f,_60){
return this._vLineToA(this._adjustRelPos(_60.y,_5f),_60);
},_curveToA:function(_61){
var p=[],n=_61 instanceof Array?_61:_61.args,l=n.length,lc=this.lastControl;
for(var i=0;i<l;i+=6){
p.push(" c");
this._addArgs(p,n,i,i+6);
}
lc.x=n[l-4];
lc.y=n[l-3];
lc.type="C";
return p;
},_curveToR:function(_62,_63){
return this._curveToA(this._adjustRelCrd(_63,_62,6));
},_smoothCurveToA:function(_64,_65){
var p=[],n=_64 instanceof Array?_64:_64.args,l=n.length,lc=this.lastControl,i=0;
if(lc.type!="C"){
p.push(" c");
this._addArgs(p,[_65.x,_65.y],0,2);
this._addArgs(p,n,0,4);
lc.x=n[0];
lc.y=n[1];
lc.type="C";
i=4;
}
for(;i<l;i+=4){
p.push(" c");
this._addArgs(p,[2*_65.x-lc.x,2*_65.y-lc.y],0,2);
this._addArgs(p,n,i,i+4);
lc.x=n[i];
lc.y=n[i+1];
}
return p;
},_smoothCurveToR:function(_66,_67){
return this._smoothCurveToA(this._adjustRelCrd(_67,_66,4),_67);
},_qCurveToA:function(_68){
var p=[],n=_68 instanceof Array?_68:_68.args,l=n.length,lc=this.lastControl;
for(var i=0;i<l;i+=4){
p.push(" qb");
this._addArgs(p,n,i,i+4);
}
lc.x=n[l-4];
lc.y=n[l-3];
lc.type="Q";
return p;
},_qCurveToR:function(_69,_6a){
return this._qCurveToA(this._adjustRelCrd(_6a,_69,4));
},_qSmoothCurveToA:function(_6b,_6c){
var p=[],n=_6b instanceof Array?_6b:_6b.args,l=n.length,lc=this.lastControl,i=0;
if(lc.type!="Q"){
p.push(" qb");
this._addArgs(p,[lc.x=_6c.x,lc.y=_6c.y],0,2);
lc.type="Q";
this._addArgs(p,n,0,2);
i=2;
}
for(;i<l;i+=2){
p.push(" qb");
this._addArgs(p,[lc.x=2*_6c.x-lc.x,lc.y=2*_6c.y-lc.y],0,2);
this._addArgs(p,n,i,i+2);
}
return p;
},_qSmoothCurveToR:function(_6d,_6e){
return this._qSmoothCurveToA(this._adjustRelCrd(_6e,_6d,2),_6e);
},_arcTo:function(_6f,_70){
var p=[],n=_6f.args,l=n.length,_71=_6f.action=="a";
for(var i=0;i<l;i+=7){
var x1=n[i+5],y1=n[i+6];
if(_71){
x1+=_70.x;
y1+=_70.y;
}
var _72=_b.arcAsBezier(_70,n[i],n[i+1],n[i+2],n[i+3]?1:0,n[i+4]?1:0,x1,y1);
for(var j=0;j<_72.length;++j){
p.push(" c");
var t=_72[j];
this._addArgs(p,t,0,t.length);
this._updateBBox(t[0],t[1]);
this._updateBBox(t[2],t[3]);
this._updateBBox(t[4],t[5]);
}
_70.x=x1;
_70.y=y1;
}
this.lastControl.type="";
return p;
},_closePath:function(){
this.lastControl.type="";
return ["x"];
}});
_d.Path.nodeType="shape";
_2("dojox.gfx.vml.TextPath",[_d.Path,_a.TextPath],{constructor:function(_73){
if(_73){
_73.setAttribute("dojoGfxType","textpath");
}
this.fontStyle=null;
if(!("text" in this)){
this.text=_1.clone(g.defaultTextPath);
}
if(!("fontStyle" in this)){
this.fontStyle=_1.clone(g.defaultFont);
}
},setText:function(_74){
this.text=g.makeParameters(this.text,typeof _74=="string"?{text:_74}:_74);
this._setText();
return this;
},setFont:function(_75){
this.fontStyle=typeof _75=="string"?g.splitFontString(_75):g.makeParameters(g.defaultFont,_75);
this._setFont();
return this;
},_setText:function(){
this.bbox=null;
var r=this.rawNode,s=this.text,p=null,t=null,c=r.childNodes;
for(var i=0;i<c.length;++i){
var tag=c[i].tagName;
if(tag=="path"){
p=c[i];
if(t){
break;
}
}else{
if(tag=="textpath"){
t=c[i];
if(p){
break;
}
}
}
}
if(!p){
p=this.rawNode.ownerDocument.createElement("v:path");
r.appendChild(p);
}
if(!t){
t=this.rawNode.ownerDocument.createElement("v:textpath");
r.appendChild(t);
}
p.textPathOk=true;
t.on=true;
var a=_d.text_alignment[s.align];
t.style["v-text-align"]=a?a:"left";
t.style["text-decoration"]=s.decoration;
t.style["v-rotate-letters"]=s.rotated;
t.style["v-text-kern"]=s.kerning;
t.string=s.text;
},_setFont:function(){
var f=this.fontStyle,c=this.rawNode.childNodes;
for(var i=0;i<c.length;++i){
if(c[i].tagName=="textpath"){
c[i].style.font=g.makeFontString(f);
break;
}
}
}});
_d.TextPath.nodeType="shape";
_2("dojox.gfx.vml.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},setDimensions:function(_76,_77){
this.width=g.normalizedLength(_76);
this.height=g.normalizedLength(_77);
if(!this.rawNode){
return this;
}
var cs=this.clipNode.style,r=this.rawNode,rs=r.style,bs=this.bgNode.style,ps=this._parent.style,i;
ps.width=_76;
ps.height=_77;
cs.width=_76;
cs.height=_77;
cs.clip="rect(0px "+_76+"px "+_77+"px 0px)";
rs.width=_76;
rs.height=_77;
r.coordsize=_76+" "+_77;
bs.width=_76;
bs.height=_77;
for(i=0;i<this.children.length;++i){
this.children[i]._setDimensions(_76,_77);
}
return this;
},getDimensions:function(){
var t=this.rawNode?{width:g.normalizedLength(this.rawNode.style.width),height:g.normalizedLength(this.rawNode.style.height)}:null;
if(t.width<=0){
t.width=this.width;
}
if(t.height<=0){
t.height=this.height;
}
return t;
}});
_d.createSurface=function(_78,_79,_7a){
if(!_79&&!_7a){
var pos=_8.position(_78);
_79=_79||pos.w;
_7a=_7a||pos.h;
}
if(typeof _79=="number"){
_79=_79+"px";
}
if(typeof _7a=="number"){
_7a=_7a+"px";
}
var s=new _d.Surface(),p=_7.byId(_78),c=s.clipNode=p.ownerDocument.createElement("div"),r=s.rawNode=p.ownerDocument.createElement("v:group"),cs=c.style,rs=r.style;
if(_5("ie")>7){
rs.display="inline-block";
}
s._parent=p;
s._nodes.push(c);
p.style.width=_79;
p.style.height=_7a;
cs.position="absolute";
cs.width=_79;
cs.height=_7a;
cs.clip="rect(0px "+_79+" "+_7a+" 0px)";
rs.position="absolute";
rs.width=_79;
rs.height=_7a;
r.coordsize=(_79==="100%"?_79:parseFloat(_79))+" "+(_7a==="100%"?_7a:parseFloat(_7a));
r.coordorigin="0 0";
var b=s.bgNode=r.ownerDocument.createElement("v:rect"),bs=b.style;
bs.left=bs.top=0;
bs.width=rs.width;
bs.height=rs.height;
b.filled=b.stroked="f";
r.appendChild(b);
c.appendChild(r);
p.appendChild(c);
s.width=g.normalizedLength(_79);
s.height=g.normalizedLength(_7a);
return s;
};
function _7b(_7c,f,o){
o=o||_9.global;
f.call(o,_7c);
if(_7c instanceof g.Surface||_7c instanceof g.Group){
_3.forEach(_7c.children,function(_7d){
_7b(_7d,f,o);
});
}
};
var _7e=function(_7f){
if(this!=_7f.getParent()){
var _80=_7f.getParent();
if(_80){
_80.remove(_7f);
}
this.rawNode.appendChild(_7f.rawNode);
C.add.apply(this,arguments);
_7b(this,function(s){
if(typeof (s.getFont)=="function"){
s.setShape(s.getShape());
s.setFont(s.getFont());
}
if(typeof (s.setFill)=="function"){
s.setFill(s.getFill());
s.setStroke(s.getStroke());
}
});
}
return this;
};
var _81=function(_82){
if(this!=_82.getParent()){
this.rawNode.appendChild(_82.rawNode);
if(!_82.getParent()){
_82.setFill(_82.getFill());
_82.setStroke(_82.getStroke());
}
C.add.apply(this,arguments);
}
return this;
};
var C=gs.Container,_83={add:_6.fixVmlAdd===true?_7e:_81,remove:function(_84,_85){
if(this==_84.getParent()){
if(this.rawNode==_84.rawNode.parentNode){
this.rawNode.removeChild(_84.rawNode);
}
C.remove.apply(this,arguments);
}
return this;
},clear:function(){
var r=this.rawNode;
while(r.firstChild!=r.lastChild){
if(r.firstChild!=this.bgNode){
r.removeChild(r.firstChild);
}
if(r.lastChild!=this.bgNode){
r.removeChild(r.lastChild);
}
}
return C.clear.apply(this,arguments);
},_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _86={createGroup:function(){
var _87=this.createObject(_d.Group,null);
var r=_87.rawNode.ownerDocument.createElement("v:rect");
r.style.left=r.style.top=0;
r.style.width=_87.rawNode.style.width;
r.style.height=_87.rawNode.style.height;
r.filled=r.stroked="f";
_87.rawNode.appendChild(r);
_87.bgNode=r;
return _87;
},createImage:function(_88){
if(!this.rawNode){
return null;
}
var _89=new _d.Image(),doc=this.rawNode.ownerDocument,_8a=doc.createElement("v:rect");
_8a.stroked="f";
_8a.style.width=this.rawNode.style.width;
_8a.style.height=this.rawNode.style.height;
var img=doc.createElement("v:imagedata");
_8a.appendChild(img);
_89.setRawNode(_8a);
this.rawNode.appendChild(_8a);
_89.setShape(_88);
this.add(_89);
return _89;
},createRect:function(_8b){
if(!this.rawNode){
return null;
}
var _8c=new _d.Rect,_8d=this.rawNode.ownerDocument.createElement("v:roundrect");
if(_5("ie")>7){
_8d.style.display="inline-block";
}
_8c.setRawNode(_8d);
this.rawNode.appendChild(_8d);
_8c.setShape(_8b);
this.add(_8c);
return _8c;
},createObject:function(_8e,_8f){
if(!this.rawNode){
return null;
}
var _90=new _8e(),_91=this.rawNode.ownerDocument.createElement("v:"+_8e.nodeType);
_90.setRawNode(_91);
this.rawNode.appendChild(_91);
switch(_8e){
case _d.Group:
case _d.Line:
case _d.Polyline:
case _d.Image:
case _d.Text:
case _d.Path:
case _d.TextPath:
this._overrideSize(_91);
}
_90.setShape(_8f);
this.add(_90);
return _90;
},_overrideSize:function(_92){
var s=this.rawNode.style,w=s.width,h=s.height;
_92.style.width=w;
_92.style.height=h;
_92.coordsize=parseInt(w)+" "+parseInt(h);
}};
_1.extend(_d.Group,_83);
_1.extend(_d.Group,gs.Creator);
_1.extend(_d.Group,_86);
_1.extend(_d.Surface,_83);
_1.extend(_d.Surface,gs.Creator);
_1.extend(_d.Surface,_86);
_d.fixTarget=function(_93,_94){
if(!_93.gfxTarget){
_93.gfxTarget=gs.byId(_93.target.__gfxObject__);
}
return true;
};
return _d;
});
