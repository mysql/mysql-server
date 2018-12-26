/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/autoscroll",["../main","../window"],function(_1){
_1.getObject("dnd",true,_1);
_1.dnd.getViewport=_1.window.getBox;
_1.dnd.V_TRIGGER_AUTOSCROLL=32;
_1.dnd.H_TRIGGER_AUTOSCROLL=32;
_1.dnd.V_AUTOSCROLL_VALUE=16;
_1.dnd.H_AUTOSCROLL_VALUE=16;
_1.dnd.autoScroll=function(e){
var v=_1.window.getBox(),dx=0,dy=0;
if(e.clientX<_1.dnd.H_TRIGGER_AUTOSCROLL){
dx=-_1.dnd.H_AUTOSCROLL_VALUE;
}else{
if(e.clientX>v.w-_1.dnd.H_TRIGGER_AUTOSCROLL){
dx=_1.dnd.H_AUTOSCROLL_VALUE;
}
}
if(e.clientY<_1.dnd.V_TRIGGER_AUTOSCROLL){
dy=-_1.dnd.V_AUTOSCROLL_VALUE;
}else{
if(e.clientY>v.h-_1.dnd.V_TRIGGER_AUTOSCROLL){
dy=_1.dnd.V_AUTOSCROLL_VALUE;
}
}
window.scrollBy(dx,dy);
};
_1.dnd._validNodes={"div":1,"p":1,"td":1};
_1.dnd._validOverflow={"auto":1,"scroll":1};
_1.dnd.autoScrollNodes=function(e){
var b,t,w,h,rx,ry,dx=0,dy=0,_2,_3;
for(var n=e.target;n;){
if(n.nodeType==1&&(n.tagName.toLowerCase() in _1.dnd._validNodes)){
var s=_1.getComputedStyle(n),_4=(s.overflow.toLowerCase() in _1.dnd._validOverflow),_5=(s.overflowX.toLowerCase() in _1.dnd._validOverflow),_6=(s.overflowY.toLowerCase() in _1.dnd._validOverflow);
if(_4||_5||_6){
b=_1._getContentBox(n,s);
t=_1.position(n,true);
}
if(_4||_5){
w=Math.min(_1.dnd.H_TRIGGER_AUTOSCROLL,b.w/2);
rx=e.pageX-t.x;
if(_1.isWebKit||_1.isOpera){
rx+=_1.body().scrollLeft;
}
dx=0;
if(rx>0&&rx<b.w){
if(rx<w){
dx=-w;
}else{
if(rx>b.w-w){
dx=w;
}
}
_2=n.scrollLeft;
n.scrollLeft=n.scrollLeft+dx;
}
}
if(_4||_6){
h=Math.min(_1.dnd.V_TRIGGER_AUTOSCROLL,b.h/2);
ry=e.pageY-t.y;
if(_1.isWebKit||_1.isOpera){
ry+=_1.body().scrollTop;
}
dy=0;
if(ry>0&&ry<b.h){
if(ry<h){
dy=-h;
}else{
if(ry>b.h-h){
dy=h;
}
}
_3=n.scrollTop;
n.scrollTop=n.scrollTop+dy;
}
}
if(dx||dy){
return;
}
}
try{
n=n.parentNode;
}
catch(x){
n=null;
}
}
_1.dnd.autoScroll(e);
};
return _1.dnd;
});
