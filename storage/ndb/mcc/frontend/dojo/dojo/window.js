/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/window",["./_base/lang","./sniff","./_base/window","./dom","./dom-geometry","./dom-style","./dom-construct"],function(_1,_2,_3,_4,_5,_6,_7){
_2.add("rtl-adjust-position-for-verticalScrollBar",function(_8,_9){
var _a=_3.body(_9),_b=_7.create("div",{style:{overflow:"scroll",overflowX:"visible",direction:"rtl",visibility:"hidden",position:"absolute",left:"0",top:"0",width:"64px",height:"64px"}},_a,"last"),_c=_7.create("div",{style:{overflow:"hidden",direction:"ltr"}},_b,"last"),_d=_5.position(_c).x!=0;
_b.removeChild(_c);
_a.removeChild(_b);
return _d;
});
_2.add("position-fixed-support",function(_e,_f){
var _10=_3.body(_f),_11=_7.create("span",{style:{visibility:"hidden",position:"fixed",left:"1px",top:"1px"}},_10,"last"),_12=_7.create("span",{style:{position:"fixed",left:"0",top:"0"}},_11,"last"),ret=_5.position(_12).x!=_5.position(_11).x;
_11.removeChild(_12);
_10.removeChild(_11);
return ret;
});
var _13={getBox:function(doc){
doc=doc||_3.doc;
var _14=(doc.compatMode=="BackCompat")?_3.body(doc):doc.documentElement,_15=_5.docScroll(doc),w,h;
if(_2("touch")){
var _16=_13.get(doc);
w=_16.innerWidth||_14.clientWidth;
h=_16.innerHeight||_14.clientHeight;
}else{
w=_14.clientWidth;
h=_14.clientHeight;
}
return {l:_15.x,t:_15.y,w:w,h:h};
},get:function(doc){
if(_2("ie")&&_13!==document.parentWindow){
doc.parentWindow.execScript("document._parentWindow = window;","Javascript");
var win=doc._parentWindow;
doc._parentWindow=null;
return win;
}
return doc.parentWindow||doc.defaultView;
},scrollIntoView:function(_17,pos){
try{
_17=_4.byId(_17);
var doc=_17.ownerDocument||_3.doc,_18=_3.body(doc),_19=doc.documentElement||_18.parentNode,_1a=_2("ie")||_2("trident"),_1b=_2("webkit");
if(_17==_18||_17==_19){
return;
}
if(!(_2("mozilla")||_1a||_1b||_2("opera")||_2("trident")||_2("edge"))&&("scrollIntoView" in _17)){
_17.scrollIntoView(false);
return;
}
var _1c=doc.compatMode=="BackCompat",_1d=Math.min(_18.clientWidth||_19.clientWidth,_19.clientWidth||_18.clientWidth),_1e=Math.min(_18.clientHeight||_19.clientHeight,_19.clientHeight||_18.clientHeight),_1f=(_1b||_1c)?_18:_19,_20=pos||_5.position(_17),el=_17.parentNode,_21=function(el){
return (_1a<=6||(_1a==7&&_1c))?false:(_2("position-fixed-support")&&(_6.get(el,"position").toLowerCase()=="fixed"));
},_22=this,_23=function(el,x,y){
if(el.tagName=="BODY"||el.tagName=="HTML"){
_22.get(el.ownerDocument).scrollBy(x,y);
}else{
x&&(el.scrollLeft+=x);
y&&(el.scrollTop+=y);
}
};
if(_21(_17)){
return;
}
while(el){
if(el==_18){
el=_1f;
}
var _24=_5.position(el),_25=_21(el),rtl=_6.getComputedStyle(el).direction.toLowerCase()=="rtl";
if(el==_1f){
_24.w=_1d;
_24.h=_1e;
if(_1f==_19&&(_1a||_2("trident"))&&rtl){
_24.x+=_1f.offsetWidth-_24.w;
}
_24.x=0;
_24.y=0;
}else{
var pb=_5.getPadBorderExtents(el);
_24.w-=pb.w;
_24.h-=pb.h;
_24.x+=pb.l;
_24.y+=pb.t;
var _26=el.clientWidth,_27=_24.w-_26;
if(_26>0&&_27>0){
if(rtl&&_2("rtl-adjust-position-for-verticalScrollBar")){
_24.x+=_27;
}
_24.w=_26;
}
_26=el.clientHeight;
_27=_24.h-_26;
if(_26>0&&_27>0){
_24.h=_26;
}
}
if(_25){
if(_24.y<0){
_24.h+=_24.y;
_24.y=0;
}
if(_24.x<0){
_24.w+=_24.x;
_24.x=0;
}
if(_24.y+_24.h>_1e){
_24.h=_1e-_24.y;
}
if(_24.x+_24.w>_1d){
_24.w=_1d-_24.x;
}
}
var l=_20.x-_24.x,t=_20.y-_24.y,r=l+_20.w-_24.w,bot=t+_20.h-_24.h;
var s,old;
if(r*l>0&&(!!el.scrollLeft||el==_1f||el.scrollWidth>el.offsetHeight)){
s=Math[l<0?"max":"min"](l,r);
if(rtl&&((_1a==8&&!_1c)||_2("trident")>=5)){
s=-s;
}
old=el.scrollLeft;
_23(el,s,0);
s=el.scrollLeft-old;
_20.x-=s;
}
if(bot*t>0&&(!!el.scrollTop||el==_1f||el.scrollHeight>el.offsetHeight)){
s=Math.ceil(Math[t<0?"max":"min"](t,bot));
old=el.scrollTop;
_23(el,0,s);
s=el.scrollTop-old;
_20.y-=s;
}
el=(el!=_1f)&&!_25&&el.parentNode;
}
}
catch(error){
console.error("scrollIntoView: "+error);
_17.scrollIntoView(false);
}
}};
1&&_1.setObject("dojo.window",_13);
return _13;
});
