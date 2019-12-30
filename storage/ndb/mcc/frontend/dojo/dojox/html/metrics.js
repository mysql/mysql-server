//>>built
define("dojox/html/metrics",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/sniff","dojo/ready","dojo/_base/unload","dojo/_base/window","dojo/dom-geometry"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_2.getObject("dojox.html.metrics",true);
var _9=_2.getObject("dojox");
_8.getFontMeasurements=function(){
var _a={"1em":0,"1ex":0,"100%":0,"12pt":0,"16px":0,"xx-small":0,"x-small":0,"small":0,"medium":0,"large":0,"x-large":0,"xx-large":0};
var _b;
if(_3("ie")){
_b=_6.doc.documentElement.style.fontSize||"";
if(!_b){
_6.doc.documentElement.style.fontSize="100%";
}
}
var _c=_6.doc.createElement("div");
var ds=_c.style;
ds.position="absolute";
ds.left="-100px";
ds.top="0";
ds.width="30px";
ds.height="1000em";
ds.borderWidth="0";
ds.margin="0";
ds.padding="0";
ds.outline="0";
ds.lineHeight="1";
ds.overflow="hidden";
_6.body().appendChild(_c);
for(var p in _a){
ds.fontSize=p;
_a[p]=Math.round(_c.offsetHeight*12/16)*16/12/1000;
}
if(_3("ie")){
_6.doc.documentElement.style.fontSize=_b;
}
_6.body().removeChild(_c);
_c=null;
return _a;
};
var _d=null;
_8.getCachedFontMeasurements=function(_e){
if(_e||!_d){
_d=_8.getFontMeasurements();
}
return _d;
};
var _f=null,_10={};
_8.getTextBox=function(_11,_12,_13){
var m,s;
if(!_f){
m=_f=_6.doc.createElement("div");
var c=_6.doc.createElement("div");
c.appendChild(m);
s=c.style;
s.overflow="scroll";
s.position="absolute";
s.left="0px";
s.top="-10000px";
s.width="1px";
s.height="1px";
s.visibility="hidden";
s.borderWidth="0";
s.margin="0";
s.padding="0";
s.outline="0";
_6.body().appendChild(c);
}else{
m=_f;
}
m.className="";
s=m.style;
s.borderWidth="0";
s.margin="0";
s.padding="0";
s.outline="0";
if(arguments.length>1&&_12){
for(var i in _12){
if(i in _10){
continue;
}
s[i]=_12[i];
}
}
if(arguments.length>2&&_13){
m.className=_13;
}
m.innerHTML=_11;
var box=_7.position(m);
box.w=m.parentNode.scrollWidth;
return box;
};
var _14={w:16,h:16};
_8.getScrollbar=function(){
return {w:_14.w,h:_14.h};
};
_8._fontResizeNode=null;
_8.initOnFontResize=function(_15){
var f=_8._fontResizeNode=_6.doc.createElement("iframe");
var fs=f.style;
fs.position="absolute";
fs.width="5em";
fs.height="10em";
fs.top="-10000px";
fs.display="none";
if(_3("ie")){
f.onreadystatechange=function(){
if(f.contentWindow.document.readyState=="complete"){
f.onresize=f.contentWindow.parent[_9._scopeName].html.metrics._fontresize;
}
};
}else{
f.onload=function(){
f.contentWindow.onresize=f.contentWindow.parent[_9._scopeName].html.metrics._fontresize;
};
}
f.setAttribute("src","javascript:'<html><head><script>if(\"loadFirebugConsole\" in window){window.loadFirebugConsole();}</script></head><body></body></html>'");
_6.body().appendChild(f);
_8.initOnFontResize=function(){
};
};
_8.onFontResize=function(){
};
_8._fontresize=function(){
_8.onFontResize();
};
_5.addOnUnload(function(){
var f=_8._fontResizeNode;
if(f){
if(_3("ie")&&f.onresize){
f.onresize=null;
}else{
if(f.contentWindow&&f.contentWindow.onresize){
f.contentWindow.onresize=null;
}
}
_8._fontResizeNode=null;
}
});
_4(function(){
try{
var n=_6.doc.createElement("div");
n.style.cssText="top:0;left:0;width:100px;height:100px;overflow:scroll;position:absolute;visibility:hidden;";
_6.body().appendChild(n);
_14.w=n.offsetWidth-n.clientWidth;
_14.h=n.offsetHeight-n.clientHeight;
_6.body().removeChild(n);
delete n;
}
catch(e){
}
if("fontSizeWatch" in _1.config&&!!_1.config.fontSizeWatch){
_8.initOnFontResize();
}
});
return _8;
});
