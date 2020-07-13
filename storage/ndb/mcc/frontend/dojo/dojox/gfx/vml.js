//>>built
define("dojox/gfx/vml",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/Color","dojo/_base/sniff","dojo/_base/config","dojo/dom","dojo/dom-geometry","dojo/_base/kernel","./_base","./shape","./path","./arc","./gradient","./matrix"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,g,gs,_a,_b,_c,m){
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
_d._reparentEvents=function(dst,src){
for(var _10 in src){
if(_10.substr(0,2).toLowerCase()=="on"){
dst[_10]=src[_10];
src[_10]=null;
}
}
};
_d.Shape=_2("dojox.gfx.vml.Shape",gs.Shape,{setFill:function(_11){
if(!_11){
this.fillStyle=null;
this.rawNode.filled="f";
return this;
}
var i,f,fo,a,s;
if(typeof _11=="object"&&"type" in _11){
switch(_11.type){
case "linear":
var _12=this._getRealMatrix(),_13=this.getBoundingBox(),_14=this._getRealBBox?this._getRealBBox():this.getTransformedBoundingBox();
s=[];
if(this.fillStyle!==_11){
this.fillStyle=g.makeParameters(g.defaultLinearGradient,_11);
}
f=g.gradient.project(_12,this.fillStyle,{x:_13.x,y:_13.y},{x:_13.x+_13.width,y:_13.y+_13.height},_14[0],_14[2]);
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
f=g.makeParameters(g.defaultRadialGradient,_11);
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
f=g.makeParameters(g.defaultPattern,_11);
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
this.fillStyle=g.normalizeColor(_11);
fo=this.rawNode.fill;
if(!fo){
fo=this.rawNode.ownerDocument.createElement("v:fill");
}
fo.method="any";
fo.type="solid";
fo.opacity=this.fillStyle.a;
var _15=this.rawNode.filters["DXImageTransform.Microsoft.Alpha"];
if(_15){
_15.opacity=Math.round(this.fillStyle.a*100);
}
this.rawNode.fillcolor=this.fillStyle.toHex();
this.rawNode.filled=true;
return this;
},setStroke:function(_16){
if(!_16){
this.strokeStyle=null;
this.rawNode.stroked="f";
return this;
}
if(typeof _16=="string"||_1.isArray(_16)||_16 instanceof _4){
_16={color:_16};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_16);
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
},_capMap:{butt:"flat"},_capMapReversed:{flat:"butt"},_translate:function(_17,_18){
return (_18 in _17)?_17[_18]:_18;
},_applyTransform:function(){
var _19=this._getRealMatrix();
if(_19){
var _1a=this.rawNode.skew;
if(typeof _1a=="undefined"){
for(var i=0;i<this.rawNode.childNodes.length;++i){
if(this.rawNode.childNodes[i].tagName=="skew"){
_1a=this.rawNode.childNodes[i];
break;
}
}
}
if(_1a){
_1a.on="f";
var mt=_19.xx.toFixed(8)+" "+_19.xy.toFixed(8)+" "+_19.yx.toFixed(8)+" "+_19.yy.toFixed(8)+" 0 0",_1b=Math.floor(_19.dx).toFixed()+"px "+Math.floor(_19.dy).toFixed()+"px",s=this.rawNode.style,l=parseFloat(s.left),t=parseFloat(s.top),w=parseFloat(s.width),h=parseFloat(s.height);
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
var _1c=(-l/w-0.5).toFixed(8)+" "+(-t/h-0.5).toFixed(8);
_1a.matrix=mt;
_1a.origin=_1c;
_1a.offset=_1b;
_1a.on=true;
}
}
if(this.fillStyle&&this.fillStyle.type=="linear"){
this.setFill(this.fillStyle);
}
if(this.clip){
this.setClip(this.clip);
}
return this;
},_setDimensions:function(_1d,_1e){
return this;
},setRawNode:function(_1f){
_1f.stroked="f";
_1f.filled="f";
this.rawNode=_1f;
this.rawNode.__gfxObject__=this;
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
},setClip:function(_20){
this.inherited(arguments);
var _21=this.rawNode.style;
if(!_20){
_21.position="absolute";
_21.clip="rect(0px "+_21.width+" "+_21.height+" 0px)";
}else{
if("width" in _20){
var _22=this._getRealMatrix(),l=parseFloat(_21.left),t=parseFloat(_21.top);
if(isNaN(l)){
l=0;
}
if(isNaN(t)){
t=0;
}
var _23=m.multiplyRectangle(_22,_20);
var pt=m.multiplyPoint(_22,{x:l,y:t});
_21.clip="rect("+Math.round(_23.y-pt.y)+"px "+Math.round(_23.x-pt.x+_23.width)+"px "+Math.round(_23.y-pt.y+_23.height)+"px "+Math.round(_23.x-pt.x)+"px)";
}
}
return this;
}});
_d.Group=_2("dojox.gfx.vml.Group",_d.Shape,{constructor:function(){
gs.Container._init.call(this);
},_applyTransform:function(){
var _24=this._getRealMatrix();
for(var i=0;i<this.children.length;++i){
this.children[i]._updateParentMatrix(_24);
}
if(this.clip){
this.setClip(this.clip);
}
return this;
},_setDimensions:function(_25,_26){
var r=this.rawNode,rs=r.style,bs=this.bgNode.style;
rs.width=_25;
rs.height=_26;
r.coordsize=_25+" "+_26;
bs.width=_25;
bs.height=_26;
for(var i=0;i<this.children.length;++i){
this.children[i]._setDimensions(_25,_26);
}
return this;
},setClip:function(_27){
this.clip=_27;
var _28=this.rawNode.style;
if(!_27){
_28.position="absolute";
_28.clip="rect(0px "+_28.width+" "+_28.height+" 0px)";
}else{
if("width" in _27){
var _29=this._getRealMatrix();
var _2a=m.multiplyRectangle(_29,_27);
var _2b=this.getBoundingBox();
_2b=_2b?m.multiplyRectangle(_29,_2b):null;
var _2c=_2b&&_2b.x<0?_2b.x:0,_2d=_2b&&_2b.y<0?_2b.y:0;
_28.position="absolute";
_28.clip="rect("+Math.round(_2a.y-_2d)+"px "+Math.round(_2a.x+_2a.width-_2c)+"px "+Math.round(_2a.y+_2a.height-_2d)+"px "+Math.round(_2a.x-_2c)+"px)";
}
}
return this;
},destroy:function(){
this.clear(true);
_d.Shape.prototype.destroy.apply(this,arguments);
}});
_d.Group.nodeType="group";
_d.Rect=_2("dojox.gfx.vml.Rect",[_d.Shape,gs.Rect],{setShape:function(_2e){
var _2f=this.shape=g.makeParameters(this.shape,_2e);
this.bbox=null;
var r=Math.min(1,(_2f.r/Math.min(parseFloat(_2f.width),parseFloat(_2f.height)))).toFixed(8);
var _30=this.rawNode.parentNode,_31=null;
if(_30){
if(_30.lastChild!==this.rawNode){
for(var i=0;i<_30.childNodes.length;++i){
if(_30.childNodes[i]===this.rawNode){
_31=_30.childNodes[i+1];
break;
}
}
}
_30.removeChild(this.rawNode);
}
if(_5("ie")>7){
var _32=this.rawNode.ownerDocument.createElement("v:roundrect");
_32.arcsize=r;
_32.style.display="inline-block";
_d._reparentEvents(_32,this.rawNode);
this.rawNode=_32;
this.rawNode.__gfxObject__=this;
}else{
this.rawNode.arcsize=r;
}
if(_30){
if(_31){
_30.insertBefore(this.rawNode,_31);
}else{
_30.appendChild(this.rawNode);
}
}
var _33=this.rawNode.style;
_33.left=_2f.x.toFixed();
_33.top=_2f.y.toFixed();
_33.width=(typeof _2f.width=="string"&&_2f.width.indexOf("%")>=0)?_2f.width:Math.max(_2f.width.toFixed(),0);
_33.height=(typeof _2f.height=="string"&&_2f.height.indexOf("%")>=0)?_2f.height:Math.max(_2f.height.toFixed(),0);
return this.setTransform(this.matrix).setFill(this.fillStyle).setStroke(this.strokeStyle);
}});
_d.Rect.nodeType="roundrect";
_d.Ellipse=_2("dojox.gfx.vml.Ellipse",[_d.Shape,gs.Ellipse],{setShape:function(_34){
var _35=this.shape=g.makeParameters(this.shape,_34);
this.bbox=null;
var _36=this.rawNode.style;
_36.left=(_35.cx-_35.rx).toFixed();
_36.top=(_35.cy-_35.ry).toFixed();
_36.width=(_35.rx*2).toFixed();
_36.height=(_35.ry*2).toFixed();
return this.setTransform(this.matrix);
}});
_d.Ellipse.nodeType="oval";
_d.Circle=_2("dojox.gfx.vml.Circle",[_d.Shape,gs.Circle],{setShape:function(_37){
var _38=this.shape=g.makeParameters(this.shape,_37);
this.bbox=null;
var _39=this.rawNode.style;
_39.left=(_38.cx-_38.r).toFixed();
_39.top=(_38.cy-_38.r).toFixed();
_39.width=(_38.r*2).toFixed();
_39.height=(_38.r*2).toFixed();
return this;
}});
_d.Circle.nodeType="oval";
_d.Line=_2("dojox.gfx.vml.Line",[_d.Shape,gs.Line],{constructor:function(_3a){
if(_3a){
_3a.setAttribute("dojoGfxType","line");
}
},setShape:function(_3b){
var _3c=this.shape=g.makeParameters(this.shape,_3b);
this.bbox=null;
this.rawNode.path.v="m"+_3c.x1.toFixed()+" "+_3c.y1.toFixed()+"l"+_3c.x2.toFixed()+" "+_3c.y2.toFixed()+"e";
return this.setTransform(this.matrix);
}});
_d.Line.nodeType="shape";
_d.Polyline=_2("dojox.gfx.vml.Polyline",[_d.Shape,gs.Polyline],{constructor:function(_3d){
if(_3d){
_3d.setAttribute("dojoGfxType","polyline");
}
},setShape:function(_3e,_3f){
if(_3e&&_3e instanceof Array){
this.shape=g.makeParameters(this.shape,{points:_3e});
if(_3f&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.shape=g.makeParameters(this.shape,_3e);
}
this.bbox=null;
this._normalizePoints();
var _40=[],p=this.shape.points;
if(p.length>0){
_40.push("m");
_40.push(p[0].x.toFixed(),p[0].y.toFixed());
if(p.length>1){
_40.push("l");
for(var i=1;i<p.length;++i){
_40.push(p[i].x.toFixed(),p[i].y.toFixed());
}
}
}
_40.push("e");
this.rawNode.path.v=_40.join(" ");
return this.setTransform(this.matrix);
}});
_d.Polyline.nodeType="shape";
_d.Image=_2("dojox.gfx.vml.Image",[_d.Shape,gs.Image],{setShape:function(_41){
var _42=this.shape=g.makeParameters(this.shape,_41);
this.bbox=null;
this.rawNode.firstChild.src=_42.src;
return this.setTransform(this.matrix);
},_applyTransform:function(){
var _43=this._getRealMatrix(),_44=this.rawNode,s=_44.style,_45=this.shape;
if(_43){
_43=m.multiply(_43,{dx:_45.x,dy:_45.y});
}else{
_43=m.normalize({dx:_45.x,dy:_45.y});
}
if(_43.xy==0&&_43.yx==0&&_43.xx>0&&_43.yy>0){
s.filter="";
s.width=Math.floor(_43.xx*_45.width);
s.height=Math.floor(_43.yy*_45.height);
s.left=Math.floor(_43.dx);
s.top=Math.floor(_43.dy);
}else{
var ps=_44.parentNode.style;
s.left="0px";
s.top="0px";
s.width=ps.width;
s.height=ps.height;
_43=m.multiply(_43,{xx:_45.width/parseInt(s.width),yy:_45.height/parseInt(s.height)});
var f=_44.filters["DXImageTransform.Microsoft.Matrix"];
if(f){
f.M11=_43.xx;
f.M12=_43.xy;
f.M21=_43.yx;
f.M22=_43.yy;
f.Dx=_43.dx;
f.Dy=_43.dy;
}else{
s.filter="progid:DXImageTransform.Microsoft.Matrix(M11="+_43.xx+", M12="+_43.xy+", M21="+_43.yx+", M22="+_43.yy+", Dx="+_43.dx+", Dy="+_43.dy+")";
}
}
return this;
},_setDimensions:function(_46,_47){
var r=this.rawNode,f=r.filters["DXImageTransform.Microsoft.Matrix"];
if(f){
var s=r.style;
s.width=_46;
s.height=_47;
return this._applyTransform();
}
return this;
}});
_d.Image.nodeType="rect";
_d.Text=_2("dojox.gfx.vml.Text",[_d.Shape,gs.Text],{constructor:function(_48){
if(_48){
_48.setAttribute("dojoGfxType","text");
}
this.fontStyle=null;
},_alignment:{start:"left",middle:"center",end:"right"},setShape:function(_49){
this.shape=g.makeParameters(this.shape,_49);
this.bbox=null;
var r=this.rawNode,s=this.shape,x=s.x,y=s.y.toFixed(),_4a;
switch(s.align){
case "middle":
x-=5;
break;
case "end":
x-=10;
break;
}
_4a="m"+x.toFixed()+","+y+"l"+(x+10).toFixed()+","+y+"e";
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
p.v=_4a;
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
var _4b=this.inherited(arguments);
if(_4b){
_4b=m.multiply(_4b,{dy:-g.normalizedLength(this.fontStyle?this.fontStyle.size:"10pt")*0.35});
}
return _4b;
},getTextWidth:function(){
var _4c=this.rawNode,_4d=_4c.style.display;
_4c.style.display="inline";
var _4e=g.pt2px(parseFloat(_4c.currentStyle.width));
_4c.style.display=_4d;
return _4e;
}});
_d.Text.nodeType="shape";
_d.Path=_2("dojox.gfx.vml.Path",[_d.Shape,_a.Path],{constructor:function(_4f){
if(_4f&&!_4f.getAttribute("dojoGfxType")){
_4f.setAttribute("dojoGfxType","path");
}
this.vmlPath="";
this.lastControl={};
},_updateWithSegment:function(_50){
var _51=_1.clone(this.last);
this.inherited(arguments);
if(arguments.length>1){
return;
}
var _52=this[this.renderers[_50.action]](_50,_51);
if(typeof this.vmlPath=="string"){
this.vmlPath+=_52.join("");
this.rawNode.path.v=this.vmlPath+" r0,0 e";
}else{
Array.prototype.push.apply(this.vmlPath,_52);
}
},setShape:function(_53){
this.vmlPath=[];
this.lastControl.type="";
this.inherited(arguments);
this.vmlPath=this.vmlPath.join("");
this.rawNode.path.v=this.vmlPath+" r0,0 e";
return this;
},_pathVmlToSvgMap:{m:"M",l:"L",t:"m",r:"l",c:"C",v:"c",qb:"Q",x:"z",e:""},renderers:{M:"_moveToA",m:"_moveToR",L:"_lineToA",l:"_lineToR",H:"_hLineToA",h:"_hLineToR",V:"_vLineToA",v:"_vLineToR",C:"_curveToA",c:"_curveToR",S:"_smoothCurveToA",s:"_smoothCurveToR",Q:"_qCurveToA",q:"_qCurveToR",T:"_qSmoothCurveToA",t:"_qSmoothCurveToR",A:"_arcTo",a:"_arcTo",Z:"_closePath",z:"_closePath"},_addArgs:function(_54,_55,_56,_57){
var n=_55 instanceof Array?_55:_55.args;
for(var i=_56;i<_57;++i){
_54.push(" ",n[i].toFixed());
}
},_adjustRelCrd:function(_58,_59,_5a){
var n=_59 instanceof Array?_59:_59.args,l=n.length,_5b=new Array(l),i=0,x=_58.x,y=_58.y;
if(typeof x!="number"){
_5b[0]=x=n[0];
_5b[1]=y=n[1];
i=2;
}
if(typeof _5a=="number"&&_5a!=2){
var j=_5a;
while(j<=l){
for(;i<j;i+=2){
_5b[i]=x+n[i];
_5b[i+1]=y+n[i+1];
}
x=_5b[j-2];
y=_5b[j-1];
j+=_5a;
}
}else{
for(;i<l;i+=2){
_5b[i]=(x+=n[i]);
_5b[i+1]=(y+=n[i+1]);
}
}
return _5b;
},_adjustRelPos:function(_5c,_5d){
var n=_5d instanceof Array?_5d:_5d.args,l=n.length,_5e=new Array(l);
for(var i=0;i<l;++i){
_5e[i]=(_5c+=n[i]);
}
return _5e;
},_moveToA:function(_5f){
var p=[" m"],n=_5f instanceof Array?_5f:_5f.args,l=n.length;
this._addArgs(p,n,0,2);
if(l>2){
p.push(" l");
this._addArgs(p,n,2,l);
}
this.lastControl.type="";
return p;
},_moveToR:function(_60,_61){
return this._moveToA(this._adjustRelCrd(_61,_60));
},_lineToA:function(_62){
var p=[" l"],n=_62 instanceof Array?_62:_62.args;
this._addArgs(p,n,0,n.length);
this.lastControl.type="";
return p;
},_lineToR:function(_63,_64){
return this._lineToA(this._adjustRelCrd(_64,_63));
},_hLineToA:function(_65,_66){
var p=[" l"],y=" "+_66.y.toFixed(),n=_65 instanceof Array?_65:_65.args,l=n.length;
for(var i=0;i<l;++i){
p.push(" ",n[i].toFixed(),y);
}
this.lastControl.type="";
return p;
},_hLineToR:function(_67,_68){
return this._hLineToA(this._adjustRelPos(_68.x,_67),_68);
},_vLineToA:function(_69,_6a){
var p=[" l"],x=" "+_6a.x.toFixed(),n=_69 instanceof Array?_69:_69.args,l=n.length;
for(var i=0;i<l;++i){
p.push(x," ",n[i].toFixed());
}
this.lastControl.type="";
return p;
},_vLineToR:function(_6b,_6c){
return this._vLineToA(this._adjustRelPos(_6c.y,_6b),_6c);
},_curveToA:function(_6d){
var p=[],n=_6d instanceof Array?_6d:_6d.args,l=n.length,lc=this.lastControl;
for(var i=0;i<l;i+=6){
p.push(" c");
this._addArgs(p,n,i,i+6);
}
lc.x=n[l-4];
lc.y=n[l-3];
lc.type="C";
return p;
},_curveToR:function(_6e,_6f){
return this._curveToA(this._adjustRelCrd(_6f,_6e,6));
},_smoothCurveToA:function(_70,_71){
var p=[],n=_70 instanceof Array?_70:_70.args,l=n.length,lc=this.lastControl,i=0;
if(lc.type!="C"){
p.push(" c");
this._addArgs(p,[_71.x,_71.y],0,2);
this._addArgs(p,n,0,4);
lc.x=n[0];
lc.y=n[1];
lc.type="C";
i=4;
}
for(;i<l;i+=4){
p.push(" c");
this._addArgs(p,[2*_71.x-lc.x,2*_71.y-lc.y],0,2);
this._addArgs(p,n,i,i+4);
lc.x=n[i];
lc.y=n[i+1];
}
return p;
},_smoothCurveToR:function(_72,_73){
return this._smoothCurveToA(this._adjustRelCrd(_73,_72,4),_73);
},_qCurveToA:function(_74){
var p=[],n=_74 instanceof Array?_74:_74.args,l=n.length,lc=this.lastControl;
for(var i=0;i<l;i+=4){
p.push(" qb");
this._addArgs(p,n,i,i+4);
}
lc.x=n[l-4];
lc.y=n[l-3];
lc.type="Q";
return p;
},_qCurveToR:function(_75,_76){
return this._qCurveToA(this._adjustRelCrd(_76,_75,4));
},_qSmoothCurveToA:function(_77,_78){
var p=[],n=_77 instanceof Array?_77:_77.args,l=n.length,lc=this.lastControl,i=0;
if(lc.type!="Q"){
p.push(" qb");
this._addArgs(p,[lc.x=_78.x,lc.y=_78.y],0,2);
lc.type="Q";
this._addArgs(p,n,0,2);
i=2;
}
for(;i<l;i+=2){
p.push(" qb");
this._addArgs(p,[lc.x=2*_78.x-lc.x,lc.y=2*_78.y-lc.y],0,2);
this._addArgs(p,n,i,i+2);
}
return p;
},_qSmoothCurveToR:function(_79,_7a){
return this._qSmoothCurveToA(this._adjustRelCrd(_7a,_79,2),_7a);
},_arcTo:function(_7b,_7c){
var p=[],n=_7b.args,l=n.length,_7d=_7b.action=="a";
for(var i=0;i<l;i+=7){
var x1=n[i+5],y1=n[i+6];
if(_7d){
x1+=_7c.x;
y1+=_7c.y;
}
var _7e=_b.arcAsBezier(_7c,n[i],n[i+1],n[i+2],n[i+3]?1:0,n[i+4]?1:0,x1,y1);
for(var j=0;j<_7e.length;++j){
p.push(" c");
var t=_7e[j];
this._addArgs(p,t,0,t.length);
this._updateBBox(t[0],t[1]);
this._updateBBox(t[2],t[3]);
this._updateBBox(t[4],t[5]);
}
_7c.x=x1;
_7c.y=y1;
}
this.lastControl.type="";
return p;
},_closePath:function(){
this.lastControl.type="";
return ["x"];
},_getRealBBox:function(){
this._confirmSegmented();
if(this.tbbox){
return this.tbbox;
}
if(typeof this.shape.path=="string"){
this.shape.path="";
}
return this.inherited(arguments);
}});
_d.Path.nodeType="shape";
_d.TextPath=_2("dojox.gfx.vml.TextPath",[_d.Path,_a.TextPath],{constructor:function(_7f){
if(_7f){
_7f.setAttribute("dojoGfxType","textpath");
}
this.fontStyle=null;
if(!("text" in this)){
this.text=_1.clone(g.defaultTextPath);
}
if(!("fontStyle" in this)){
this.fontStyle=_1.clone(g.defaultFont);
}
},setText:function(_80){
this.text=g.makeParameters(this.text,typeof _80=="string"?{text:_80}:_80);
this._setText();
return this;
},setFont:function(_81){
this.fontStyle=typeof _81=="string"?g.splitFontString(_81):g.makeParameters(g.defaultFont,_81);
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
_d.Surface=_2("dojox.gfx.vml.Surface",gs.Surface,{constructor:function(){
gs.Container._init.call(this);
},destroy:function(){
this.clear(true);
this.inherited(arguments);
},setDimensions:function(_82,_83){
this.width=g.normalizedLength(_82);
this.height=g.normalizedLength(_83);
if(!this.rawNode){
return this;
}
var cs=this.clipNode.style,r=this.rawNode,rs=r.style,bs=this.bgNode.style,ps=this._parent.style,i;
ps.width=_82;
ps.height=_83;
cs.width=_82;
cs.height=_83;
cs.clip="rect(0px "+_82+"px "+_83+"px 0px)";
rs.width=_82;
rs.height=_83;
r.coordsize=_82+" "+_83;
bs.width=_82;
bs.height=_83;
for(i=0;i<this.children.length;++i){
this.children[i]._setDimensions(_82,_83);
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
_d.createSurface=function(_84,_85,_86){
if(!_85&&!_86){
var pos=_8.position(_84);
_85=_85||pos.w;
_86=_86||pos.h;
}
if(typeof _85=="number"){
_85=_85+"px";
}
if(typeof _86=="number"){
_86=_86+"px";
}
var s=new _d.Surface(),p=_7.byId(_84),c=s.clipNode=p.ownerDocument.createElement("div"),r=s.rawNode=p.ownerDocument.createElement("v:group"),cs=c.style,rs=r.style;
if(_5("ie")>7){
rs.display="inline-block";
}
s._parent=p;
s._nodes.push(c);
p.style.width=_85;
p.style.height=_86;
cs.position="absolute";
cs.width=_85;
cs.height=_86;
cs.clip="rect(0px "+_85+" "+_86+" 0px)";
rs.position="absolute";
rs.width=_85;
rs.height=_86;
r.coordsize=(_85==="100%"?_85:parseFloat(_85))+" "+(_86==="100%"?_86:parseFloat(_86));
r.coordorigin="0 0";
var b=s.bgNode=r.ownerDocument.createElement("v:rect"),bs=b.style;
bs.left=bs.top=0;
bs.width=rs.width;
bs.height=rs.height;
b.filled=b.stroked="f";
r.appendChild(b);
c.appendChild(r);
p.appendChild(c);
s.width=g.normalizedLength(_85);
s.height=g.normalizedLength(_86);
return s;
};
function _87(_88,f,o){
o=o||_9.global;
f.call(o,_88);
if(_88 instanceof g.Surface||_88 instanceof g.Group){
_3.forEach(_88.children,function(_89){
_87(_89,f,o);
});
}
};
var _8a=function(_8b){
if(this!=_8b.getParent()){
var _8c=_8b.getParent();
if(_8c){
_8c.remove(_8b);
}
this.rawNode.appendChild(_8b.rawNode);
C.add.apply(this,arguments);
_87(this,function(s){
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
var _8d=function(_8e){
if(this!=_8e.getParent()){
this.rawNode.appendChild(_8e.rawNode);
if(!_8e.getParent()){
_8e.setFill(_8e.getFill());
_8e.setStroke(_8e.getStroke());
}
C.add.apply(this,arguments);
}
return this;
};
var C=gs.Container,_8f={add:_6.fixVmlAdd===true?_8a:_8d,remove:function(_90,_91){
if(this==_90.getParent()){
if(this.rawNode==_90.rawNode.parentNode){
this.rawNode.removeChild(_90.rawNode);
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
},getBoundingBox:C.getBoundingBox,_moveChildToFront:C._moveChildToFront,_moveChildToBack:C._moveChildToBack};
var _92={createGroup:function(){
var _93=this.createObject(_d.Group,null);
var r=_93.rawNode.ownerDocument.createElement("v:rect");
r.style.left=r.style.top=0;
r.style.width=_93.rawNode.style.width;
r.style.height=_93.rawNode.style.height;
r.filled=r.stroked="f";
_93.rawNode.appendChild(r);
_93.bgNode=r;
return _93;
},createImage:function(_94){
if(!this.rawNode){
return null;
}
var _95=new _d.Image(),doc=this.rawNode.ownerDocument,_96=doc.createElement("v:rect");
_96.stroked="f";
_96.style.width=this.rawNode.style.width;
_96.style.height=this.rawNode.style.height;
var img=doc.createElement("v:imagedata");
_96.appendChild(img);
_95.setRawNode(_96);
this.rawNode.appendChild(_96);
_95.setShape(_94);
this.add(_95);
return _95;
},createRect:function(_97){
if(!this.rawNode){
return null;
}
var _98=new _d.Rect,_99=this.rawNode.ownerDocument.createElement("v:roundrect");
if(_5("ie")>7){
_99.style.display="inline-block";
}
_98.setRawNode(_99);
this.rawNode.appendChild(_99);
_98.setShape(_97);
this.add(_98);
return _98;
},createObject:function(_9a,_9b){
if(!this.rawNode){
return null;
}
var _9c=new _9a(),_9d=this.rawNode.ownerDocument.createElement("v:"+_9a.nodeType);
_9c.setRawNode(_9d);
this.rawNode.appendChild(_9d);
switch(_9a){
case _d.Group:
case _d.Line:
case _d.Polyline:
case _d.Image:
case _d.Text:
case _d.Path:
case _d.TextPath:
this._overrideSize(_9d);
}
_9c.setShape(_9b);
this.add(_9c);
return _9c;
},_overrideSize:function(_9e){
var s=this.rawNode.style,w=s.width,h=s.height;
_9e.style.width=w;
_9e.style.height=h;
_9e.coordsize=parseInt(w)+" "+parseInt(h);
}};
_1.extend(_d.Group,_8f);
_1.extend(_d.Group,gs.Creator);
_1.extend(_d.Group,_92);
_1.extend(_d.Surface,_8f);
_1.extend(_d.Surface,gs.Creator);
_1.extend(_d.Surface,_92);
_d.fixTarget=function(_9f,_a0){
if(!_9f.gfxTarget){
_9f.gfxTarget=_9f.target.__gfxObject__;
}
return true;
};
return _d;
});
