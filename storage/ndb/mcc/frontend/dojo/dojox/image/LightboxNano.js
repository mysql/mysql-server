//>>built
define("dojox/image/LightboxNano",["dojo","dojo/fx"],function(_1,fx){
var _2="absolute",_3="visibility",_4=function(){
var _5=(_1.doc.compatMode=="BackCompat")?_1.body():_1.doc.documentElement,_6=_1._docScroll();
return {w:_5.clientWidth,h:_5.clientHeight,l:_6.x,t:_6.y};
};
return _1.declare("dojox.image.LightboxNano",null,{href:"",duration:500,preloadDelay:5000,constructor:function(p,n){
var _7=this;
_1.mixin(_7,p);
n=_7._node=_1.byId(n);
if(n){
if(!/a/i.test(n.tagName)){
var a=_1.create("a",{href:_7.href,"class":n.className},n,"after");
n.className="";
a.appendChild(n);
n=a;
}
_1.style(n,"position","relative");
_7._createDiv("dojoxEnlarge",n);
_1.setSelectable(n,false);
_7._onClickEvt=_1.connect(n,"onclick",_7,"_load");
}
if(_7.href){
setTimeout(function(){
(new Image()).src=_7.href;
_7._hideLoading();
},_7.preloadDelay);
}
},destroy:function(){
var a=this._connects||[];
a.push(this._onClickEvt);
_1.forEach(a,_1.disconnect);
_1.destroy(this._node);
},_createDiv:function(_8,_9,_a){
return _1.create("div",{"class":_8,style:{position:_2,display:_a?"":"none"}},_9);
},_load:function(e){
var _b=this;
e&&_1.stopEvent(e);
if(!_b._loading){
_b._loading=true;
_b._reset();
var i=_b._img=_1.create("img",{style:{visibility:"hidden",cursor:"pointer",position:_2,top:0,left:0,zIndex:9999999}},_1.body()),ln=_b._loadingNode,n=_1.query("img",_b._node)[0]||_b._node,a=_1.position(n,true),c=_1.contentBox(n),b=_1._getBorderExtents(n);
if(ln==null){
_b._loadingNode=ln=_b._createDiv("dojoxLoading",_b._node,true);
var l=_1.marginBox(ln);
_1.style(ln,{left:parseInt((c.w-l.w)/2)+"px",top:parseInt((c.h-l.h)/2)+"px"});
}
c.x=a.x-10+b.l;
c.y=a.y-10+b.t;
_b._start=c;
_b._connects=[_1.connect(i,"onload",_b,"_show")];
i.src=_b.href;
}
},_hideLoading:function(){
if(this._loadingNode){
_1.style(this._loadingNode,"display","none");
}
this._loadingNode=false;
},_show:function(){
var _c=this,vp=_4(),w=_c._img.width,h=_c._img.height,_d=parseInt((vp.w-20)*0.9),_e=parseInt((vp.h-20)*0.9),dd=_1.doc,bg=_c._bg=_1.create("div",{style:{backgroundColor:"#000",opacity:0,position:_2,zIndex:9999998}},_1.body()),ln=_c._loadingNode;
if(_c._loadingNode){
_c._hideLoading();
}
_1.style(_c._img,{border:"10px solid #fff",visibility:"visible"});
_1.style(_c._node,_3,"hidden");
_c._loading=false;
_c._connects=_c._connects.concat([_1.connect(dd,"onmousedown",_c,"_hide"),_1.connect(dd,"onkeypress",_c,"_key"),_1.connect(window,"onresize",_c,"_sizeBg")]);
if(w>_d){
h=h*_d/w;
w=_d;
}
if(h>_e){
w=w*_e/h;
h=_e;
}
_c._end={x:(vp.w-20-w)/2+vp.l,y:(vp.h-20-h)/2+vp.t,w:w,h:h};
_c._sizeBg();
_1.fx.combine([_c._anim(_c._img,_c._coords(_c._start,_c._end)),_c._anim(bg,{opacity:0.5})]).play();
},_sizeBg:function(){
var dd=_1.doc.documentElement;
_1.style(this._bg,{top:0,left:0,width:dd.scrollWidth+"px",height:dd.scrollHeight+"px"});
},_key:function(e){
_1.stopEvent(e);
this._hide();
},_coords:function(s,e){
return {left:{start:s.x,end:e.x},top:{start:s.y,end:e.y},width:{start:s.w,end:e.w},height:{start:s.h,end:e.h}};
},_hide:function(){
var _f=this;
_1.forEach(_f._connects,_1.disconnect);
_f._connects=[];
_1.fx.combine([_f._anim(_f._img,_f._coords(_f._end,_f._start),"_reset"),_f._anim(_f._bg,{opacity:0})]).play();
},_reset:function(){
_1.style(this._node,_3,"visible");
_1.destroy(this._img);
_1.destroy(this._bg);
this._img=this._bg=null;
this._node.focus();
},_anim:function(_10,_11,_12){
return _1.animateProperty({node:_10,duration:this.duration,properties:_11,onEnd:_12?_1.hitch(this,_12):null});
},show:function(_13){
_13=_13||{};
this.href=_13.href||this.href;
var n=_1.byId(_13.origin),vp=_4();
this._node=n||_1.create("div",{style:{position:_2,width:0,hieght:0,left:(vp.l+(vp.w/2))+"px",top:(vp.t+(vp.h/2))+"px"}},_1.body());
this._load();
if(!n){
_1.destroy(this._node);
}
}});
});
