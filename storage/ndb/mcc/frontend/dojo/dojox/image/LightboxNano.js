//>>built
define("dojox/image/LightboxNano",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/fx","dojo/dom","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/dom-class","dojo/on","dojo/query","dojo/fx"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,fx){
var _b="absolute",_c="visibility";
getViewport=function(){
var _d=(document.compatMode=="BackCompat")?document.body:document.documentElement,_e=_7.docScroll();
return {w:_d.clientWidth,h:_d.clientHeight,l:_e.x,t:_e.y};
};
return _2("dojox.image.LightboxNano",null,{href:"",duration:500,preloadDelay:5000,constructor:function(p,n){
var _f=this,a;
_1.mixin(_f,p);
n=_f._node=_5.byId(n);
if(n){
if(!/a/i.test(n.tagName)){
a=_6.create("a",{href:_f.href,"class":n.className},n,"after");
n.className="";
a.appendChild(n);
n=a;
}
_8.set(n,"position","relative");
_6.create("div",{"class":"nano-enlarge",style:{position:"absolute",display:"none"}},n);
_5.setSelectable(n,false);
_f._onClickEvt=on(n,"click",_1.hitch(_f,"_load"));
}
if(_f.href){
setTimeout(function(){
(new Image()).src=_f.href;
_f._hideLoading();
},_f.preloadDelay);
}
},destroy:function(){
var a=this._connects||[];
a.push(this._onClickEvt);
_3.forEach(a,function(_10){
_10.remove();
});
_6.destroy(this._node);
},_load:function(e){
var _11=this;
e&&e.preventDefault();
if(!_11._loading){
_11._loading=true;
_11._reset();
var i=_11._img=_6.create("img",{"class":"nano-image nano-image-hidden"},document.body),l,ln=_11._loadingNode,n=_a("img",_11._node)[0]||_11._node,a=_7.position(n,true),c=_7.getContentBox(n),b=_7.getBorderExtents(n);
if(ln==null){
_11._loadingNode=ln=_6.create("div",{"class":"nano-loading",style:{position:"absolute",display:""}},_11._node,"after");
l=_7.getMarginBox(ln);
_8.set(ln,{left:parseInt((c.w-l.w)/2)+"px",top:parseInt((c.h-l.h)/2)+"px"});
}
c.x=a.x-10+b.l;
c.y=a.y-10+b.t;
_11._start=c;
_11._connects=[on(i,"load",_1.hitch(_11,"_show"))];
i.src=_11.href;
}
},_hideLoading:function(){
if(this._loadingNode){
_8.set(this._loadingNode,"display","none");
}
this._loadingNode=false;
},_show:function(){
var _12=this,vp=getViewport(),w=_12._img.width,h=_12._img.height,vpw=parseInt((vp.w-20)*0.9),vph=parseInt((vp.h-20)*0.9),bg=_12._bg=_6.create("div",{"class":"nano-background",style:{opacity:0}},document.body);
if(_12._loadingNode){
_12._hideLoading();
}
_9.remove(_12._img,"nano-image-hidden");
_8.set(_12._node,_c,"hidden");
_12._loading=false;
_12._connects=_12._connects.concat([on(document,"mousedown",_1.hitch(_12,"_hide")),on(document,"keypress",_1.hitch(_12,"_key")),on(window,"resize",_1.hitch(_12,"_sizeBg"))]);
if(w>vpw){
h=h*vpw/w;
w=vpw;
}
if(h>vph){
w=w*vph/h;
h=vph;
}
_12._end={x:(vp.w-20-w)/2+vp.l,y:(vp.h-20-h)/2+vp.t,w:w,h:h};
_12._sizeBg();
fx.combine([_12._anim(_12._img,_12._coords(_12._start,_12._end)),_12._anim(bg,{opacity:0.5})]).play();
},_sizeBg:function(){
var dd=document.documentElement;
_8.set(this._bg,{top:0,left:0,width:dd.scrollWidth+"px",height:dd.scrollHeight+"px"});
},_key:function(e){
e.preventDefault();
this._hide();
},_coords:function(s,e){
return {left:{start:s.x,end:e.x},top:{start:s.y,end:e.y},width:{start:s.w,end:e.w},height:{start:s.h,end:e.h}};
},_hide:function(){
var _13=this;
_3.forEach(_13._connects,function(_14){
_14.remove();
});
_13._connects=[];
fx.combine([_13._anim(_13._img,_13._coords(_13._end,_13._start),"_reset"),_13._anim(_13._bg,{opacity:0})]).play();
},_reset:function(){
_8.set(this._node,_c,"visible");
_6.destroy(this._img);
_6.destroy(this._bg);
this._img=this._bg=null;
this._node.focus();
},_anim:function(_15,_16,_17){
return _4.animateProperty({node:_15,duration:this.duration,properties:_16,onEnd:_17?_1.hitch(this,_17):null});
},show:function(_18){
_18=_18||{};
this.href=_18.href||this.href;
var n=_5.byId(_18.origin),vp=getViewport();
this._node=n||_6.create("div",{style:{position:_b,width:0,hieght:0,left:(vp.l+(vp.w/2))+"px",top:(vp.t+(vp.h/2))+"px"}},document.body);
this._load();
if(!n){
_6.destroy(this._node);
}
}});
});
