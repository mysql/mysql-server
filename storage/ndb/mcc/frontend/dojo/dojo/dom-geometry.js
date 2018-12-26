/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-geometry",["./_base/sniff","./_base/window","./dom","./dom-style"],function(_1,_2,_3,_4){
var _5={};
_5.boxModel="content-box";
if(_1("ie")){
_5.boxModel=document.compatMode=="BackCompat"?"border-box":"content-box";
}
_5.getPadExtents=function getPadExtents(_6,_7){
_6=_3.byId(_6);
var s=_7||_4.getComputedStyle(_6),px=_4.toPixelValue,l=px(_6,s.paddingLeft),t=px(_6,s.paddingTop),r=px(_6,s.paddingRight),b=px(_6,s.paddingBottom);
return {l:l,t:t,r:r,b:b,w:l+r,h:t+b};
};
var _8="none";
_5.getBorderExtents=function getBorderExtents(_9,_a){
_9=_3.byId(_9);
var px=_4.toPixelValue,s=_a||_4.getComputedStyle(_9),l=s.borderLeftStyle!=_8?px(_9,s.borderLeftWidth):0,t=s.borderTopStyle!=_8?px(_9,s.borderTopWidth):0,r=s.borderRightStyle!=_8?px(_9,s.borderRightWidth):0,b=s.borderBottomStyle!=_8?px(_9,s.borderBottomWidth):0;
return {l:l,t:t,r:r,b:b,w:l+r,h:t+b};
};
_5.getPadBorderExtents=function getPadBorderExtents(_b,_c){
_b=_3.byId(_b);
var s=_c||_4.getComputedStyle(_b),p=_5.getPadExtents(_b,s),b=_5.getBorderExtents(_b,s);
return {l:p.l+b.l,t:p.t+b.t,r:p.r+b.r,b:p.b+b.b,w:p.w+b.w,h:p.h+b.h};
};
_5.getMarginExtents=function getMarginExtents(_d,_e){
_d=_3.byId(_d);
var s=_e||_4.getComputedStyle(_d),px=_4.toPixelValue,l=px(_d,s.marginLeft),t=px(_d,s.marginTop),r=px(_d,s.marginRight),b=px(_d,s.marginBottom);
if(_1("webkit")&&(s.position!="absolute")){
r=l;
}
return {l:l,t:t,r:r,b:b,w:l+r,h:t+b};
};
_5.getMarginBox=function getMarginBox(_f,_10){
_f=_3.byId(_f);
var s=_10||_4.getComputedStyle(_f),me=_5.getMarginExtents(_f,s),l=_f.offsetLeft-me.l,t=_f.offsetTop-me.t,p=_f.parentNode,px=_4.toPixelValue,pcs;
if(_1("mozilla")){
var sl=parseFloat(s.left),st=parseFloat(s.top);
if(!isNaN(sl)&&!isNaN(st)){
l=sl,t=st;
}else{
if(p&&p.style){
pcs=_4.getComputedStyle(p);
if(pcs.overflow!="visible"){
l+=pcs.borderLeftStyle!=_8?px(_f,pcs.borderLeftWidth):0;
t+=pcs.borderTopStyle!=_8?px(_f,pcs.borderTopWidth):0;
}
}
}
}else{
if(_1("opera")||(_1("ie")==8&&!_1("quirks"))){
if(p){
pcs=_4.getComputedStyle(p);
l-=pcs.borderLeftStyle!=_8?px(_f,pcs.borderLeftWidth):0;
t-=pcs.borderTopStyle!=_8?px(_f,pcs.borderTopWidth):0;
}
}
}
return {l:l,t:t,w:_f.offsetWidth+me.w,h:_f.offsetHeight+me.h};
};
_5.getContentBox=function getContentBox(_11,_12){
_11=_3.byId(_11);
var s=_12||_4.getComputedStyle(_11),w=_11.clientWidth,h,pe=_5.getPadExtents(_11,s),be=_5.getBorderExtents(_11,s);
if(!w){
w=_11.offsetWidth;
h=_11.offsetHeight;
}else{
h=_11.clientHeight;
be.w=be.h=0;
}
if(_1("opera")){
pe.l+=be.l;
pe.t+=be.t;
}
return {l:pe.l,t:pe.t,w:w-pe.w-be.w,h:h-pe.h-be.h};
};
function _13(_14,l,t,w,h,u){
u=u||"px";
var s=_14.style;
if(!isNaN(l)){
s.left=l+u;
}
if(!isNaN(t)){
s.top=t+u;
}
if(w>=0){
s.width=w+u;
}
if(h>=0){
s.height=h+u;
}
};
function _15(_16){
return _16.tagName.toLowerCase()=="button"||_16.tagName.toLowerCase()=="input"&&(_16.getAttribute("type")||"").toLowerCase()=="button";
};
function _17(_18){
return _5.boxModel=="border-box"||_18.tagName.toLowerCase()=="table"||_15(_18);
};
_5.setContentSize=function setContentSize(_19,box,_1a){
_19=_3.byId(_19);
var w=box.w,h=box.h;
if(_17(_19)){
var pb=_5.getPadBorderExtents(_19,_1a);
if(w>=0){
w+=pb.w;
}
if(h>=0){
h+=pb.h;
}
}
_13(_19,NaN,NaN,w,h);
};
var _1b={l:0,t:0,w:0,h:0};
_5.setMarginBox=function setMarginBox(_1c,box,_1d){
_1c=_3.byId(_1c);
var s=_1d||_4.getComputedStyle(_1c),w=box.w,h=box.h,pb=_17(_1c)?_1b:_5.getPadBorderExtents(_1c,s),mb=_5.getMarginExtents(_1c,s);
if(_1("webkit")){
if(_15(_1c)){
var ns=_1c.style;
if(w>=0&&!ns.width){
ns.width="4px";
}
if(h>=0&&!ns.height){
ns.height="4px";
}
}
}
if(w>=0){
w=Math.max(w-pb.w-mb.w,0);
}
if(h>=0){
h=Math.max(h-pb.h-mb.h,0);
}
_13(_1c,box.l,box.t,w,h);
};
_5.isBodyLtr=function isBodyLtr(){
return (_2.body().dir||_2.doc.documentElement.dir||"ltr").toLowerCase()=="ltr";
};
_5.docScroll=function docScroll(){
var _1e=_2.doc.parentWindow||_2.doc.defaultView;
return "pageXOffset" in _1e?{x:_1e.pageXOffset,y:_1e.pageYOffset}:(_1e=_1("quirks")?_2.body():_2.doc.documentElement,{x:_5.fixIeBiDiScrollLeft(_1e.scrollLeft||0),y:_1e.scrollTop||0});
};
_5.getIeDocumentElementOffset=function getIeDocumentElementOffset(){
var de=_2.doc.documentElement;
if(_1("ie")<8){
var r=de.getBoundingClientRect(),l=r.left,t=r.top;
if(_1("ie")<7){
l+=de.clientLeft;
t+=de.clientTop;
}
return {x:l<0?0:l,y:t<0?0:t};
}else{
return {x:0,y:0};
}
};
_5.fixIeBiDiScrollLeft=function fixIeBiDiScrollLeft(_1f){
var ie=_1("ie");
if(ie&&!_5.isBodyLtr()){
var qk=_1("quirks"),de=qk?_2.body():_2.doc.documentElement;
if(ie==6&&!qk&&_2.global.frameElement&&de.scrollHeight>de.clientHeight){
_1f+=de.clientLeft;
}
return (ie<8||qk)?(_1f+de.clientWidth-de.scrollWidth):-_1f;
}
return _1f;
};
_5.position=function(_20,_21){
_20=_3.byId(_20);
var db=_2.body(),dh=db.parentNode,ret=_20.getBoundingClientRect();
ret={x:ret.left,y:ret.top,w:ret.right-ret.left,h:ret.bottom-ret.top};
if(_1("ie")){
var _22=_5.getIeDocumentElementOffset();
ret.x-=_22.x+(_1("quirks")?db.clientLeft+db.offsetLeft:0);
ret.y-=_22.y+(_1("quirks")?db.clientTop+db.offsetTop:0);
}else{
if(_1("ff")==3){
var cs=_4.getComputedStyle(dh),px=_4.toPixelValue;
ret.x-=px(dh,cs.marginLeft)+px(dh,cs.borderLeftWidth);
ret.y-=px(dh,cs.marginTop)+px(dh,cs.borderTopWidth);
}
}
if(_21){
var _23=_5.docScroll();
ret.x+=_23.x;
ret.y+=_23.y;
}
return ret;
};
_5.getMarginSize=function getMarginSize(_24,_25){
_24=_3.byId(_24);
var me=_5.getMarginExtents(_24,_25||_4.getComputedStyle(_24));
var _26=_24.getBoundingClientRect();
return {w:(_26.right-_26.left)+me.w,h:(_26.bottom-_26.top)+me.h};
};
_5.normalizeEvent=function(_27){
if(!("layerX" in _27)){
_27.layerX=_27.offsetX;
_27.layerY=_27.offsetY;
}
if(!_1("dom-addeventlistener")){
var se=_27.target;
var doc=(se&&se.ownerDocument)||document;
var _28=_1("quirks")?doc.body:doc.documentElement;
var _29=_5.getIeDocumentElementOffset();
_27.pageX=_27.clientX+_5.fixIeBiDiScrollLeft(_28.scrollLeft||0)-_29.x;
_27.pageY=_27.clientY+(_28.scrollTop||0)-_29.y;
}
};
return _5;
});
