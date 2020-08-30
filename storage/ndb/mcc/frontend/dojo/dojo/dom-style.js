/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-style",["./sniff","./dom","./_base/window"],function(_1,_2,_3){
var _4,_5={};
if(_1("webkit")){
_4=function(_6){
var s;
if(_6.nodeType==1){
var dv=_6.ownerDocument.defaultView;
s=dv.getComputedStyle(_6,null);
if(!s&&_6.style){
_6.style.display="";
s=dv.getComputedStyle(_6,null);
}
}
return s||{};
};
}else{
if(_1("ie")&&(_1("ie")<9||_1("quirks"))){
_4=function(_7){
return _7.nodeType==1&&_7.currentStyle?_7.currentStyle:{};
};
}else{
_4=function(_8){
if(_8.nodeType===1){
var dv=_8.ownerDocument.defaultView,w=dv.opener?dv:_3.global.window;
return w.getComputedStyle(_8,null);
}
return {};
};
}
}
_5.getComputedStyle=_4;
var _9;
if(!_1("ie")){
_9=function(_a,_b){
return parseFloat(_b)||0;
};
}else{
_9=function(_c,_d){
if(!_d){
return 0;
}
if(_d=="medium"){
return 4;
}
if(_d.slice&&_d.slice(-2)=="px"){
return parseFloat(_d);
}
var s=_c.style,rs=_c.runtimeStyle,cs=_c.currentStyle,_e=s.left,_f=rs.left;
rs.left=cs.left;
try{
s.left=_d;
_d=s.pixelLeft;
}
catch(e){
_d=0;
}
s.left=_e;
rs.left=_f;
return _d;
};
}
_5.toPixelValue=_9;
var _10="DXImageTransform.Microsoft.Alpha";
var af=function(n,f){
try{
return n.filters.item(_10);
}
catch(e){
return f?{}:null;
}
};
var _11=_1("ie")<9||(_1("ie")<10&&_1("quirks"))?function(_12){
try{
return af(_12).Opacity/100;
}
catch(e){
return 1;
}
}:function(_13){
return _4(_13).opacity;
};
var _14=_1("ie")<9||(_1("ie")<10&&_1("quirks"))?function(_15,_16){
if(_16===""){
_16=1;
}
var ov=_16*100,_17=_16===1;
if(_17){
_15.style.zoom="";
if(af(_15)){
_15.style.filter=_15.style.filter.replace(new RegExp("\\s*progid:"+_10+"\\([^\\)]+?\\)","i"),"");
}
}else{
_15.style.zoom=1;
if(af(_15)){
af(_15,1).Opacity=ov;
}else{
_15.style.filter+=" progid:"+_10+"(Opacity="+ov+")";
}
af(_15,1).Enabled=true;
}
if(_15.tagName.toLowerCase()=="tr"){
for(var td=_15.firstChild;td;td=td.nextSibling){
if(td.tagName.toLowerCase()=="td"){
_14(td,_16);
}
}
}
return _16;
}:function(_18,_19){
return _18.style.opacity=_19;
};
var _1a={left:true,top:true};
var _1b=/margin|padding|width|height|max|min|offset/;
function _1c(_1d,_1e,_1f){
_1e=_1e.toLowerCase();
if(_1f=="auto"){
if(_1e=="height"){
return _1d.offsetHeight;
}
if(_1e=="width"){
return _1d.offsetWidth;
}
}
if(_1e=="fontweight"){
switch(_1f){
case 700:
return "bold";
case 400:
default:
return "normal";
}
}
if(!(_1e in _1a)){
_1a[_1e]=_1b.test(_1e);
}
return _1a[_1e]?_9(_1d,_1f):_1f;
};
var _20={cssFloat:1,styleFloat:1,"float":1};
_5.get=function getStyle(_21,_22){
var n=_2.byId(_21),l=arguments.length,op=(_22=="opacity");
if(l==2&&op){
return _11(n);
}
_22=_20[_22]?"cssFloat" in n.style?"cssFloat":"styleFloat":_22;
var s=_5.getComputedStyle(n);
return (l==1)?s:_1c(n,_22,s[_22]||n.style[_22]);
};
_5.set=function setStyle(_23,_24,_25){
var n=_2.byId(_23),l=arguments.length,op=(_24=="opacity");
_24=_20[_24]?"cssFloat" in n.style?"cssFloat":"styleFloat":_24;
if(l==3){
return op?_14(n,_25):n.style[_24]=_25;
}
for(var x in _24){
_5.set(_23,x,_24[x]);
}
return _5.getComputedStyle(n);
};
return _5;
});
