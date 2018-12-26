//>>built
define("dojox/mobile/scrollable",["dojo/_base/kernel","dojo/_base/connect","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var dm=_4.getObject("dojox.mobile",true);
var _a=function(_b,_c){
this.fixedHeaderHeight=0;
this.fixedFooterHeight=0;
this.isLocalFooter=false;
this.scrollBar=true;
this.scrollDir="v";
this.weight=0.6;
this.fadeScrollBar=true;
this.disableFlashScrollBar=false;
this.threshold=4;
this.constraint=true;
this.touchNode=null;
this.isNested=false;
this.dirLock=false;
this.height="";
this.androidWorkaroud=true;
this.init=function(_d){
if(_d){
for(var p in _d){
if(_d.hasOwnProperty(p)){
this[p]=((p=="domNode"||p=="containerNode")&&typeof _d[p]=="string")?_5.doc.getElementById(_d[p]):_d[p];
}
}
}
this.touchNode=this.touchNode||this.containerNode;
this._v=(this.scrollDir.indexOf("v")!=-1);
this._h=(this.scrollDir.indexOf("h")!=-1);
this._f=(this.scrollDir=="f");
this._ch=[];
this._ch.push(_2.connect(this.touchNode,_9("touch")?"touchstart":"onmousedown",this,"onTouchStart"));
if(_9("webkit")){
this._ch.push(_2.connect(this.domNode,"webkitAnimationEnd",this,"onFlickAnimationEnd"));
this._ch.push(_2.connect(this.domNode,"webkitAnimationStart",this,"onFlickAnimationStart"));
this._aw=this.androidWorkaroud&&_9("android")>=2.2&&_9("android")<3;
if(this._aw){
this._ch.push(_2.connect(_5.global,"onresize",this,"onScreenSizeChanged"));
this._ch.push(_2.connect(_5.global,"onfocus",this,function(e){
if(this.containerNode.style.webkitTransform){
this.stopAnimation();
this.toTopLeft();
}
}));
this._sz=this.getScreenSize();
}
for(var i=0;i<3;i++){
this.setKeyframes(null,null,i);
}
}
if(_9("iphone")){
_8.set(this.containerNode,"webkitTransform","translate3d(0,0,0)");
}
this._speed={x:0,y:0};
this._appFooterHeight=0;
if(this.isTopLevel()&&!this.noResize){
this.resize();
}
var _e=this;
setTimeout(function(){
_e.flashScrollBar();
},600);
};
this.isTopLevel=function(){
return true;
};
this.cleanup=function(){
if(this._ch){
for(var i=0;i<this._ch.length;i++){
_2.disconnect(this._ch[i]);
}
this._ch=null;
}
};
this.findDisp=function(_f){
if(!_f.parentNode){
return null;
}
var _10=_f.parentNode.childNodes;
for(var i=0;i<_10.length;i++){
var n=_10[i];
if(n.nodeType===1&&_6.contains(n,"mblView")&&n.style.display!=="none"){
return n;
}
}
return _f;
};
this.getScreenSize=function(){
return {h:_5.global.innerHeight||_5.doc.documentElement.clientHeight||_5.doc.documentElement.offsetHeight,w:_5.global.innerWidth||_5.doc.documentElement.clientWidth||_5.doc.documentElement.offsetWidth};
};
this.isKeyboardShown=function(e){
if(!this._sz){
return false;
}
var sz=this.getScreenSize();
return (sz.w*sz.h)/(this._sz.w*this._sz.h)<0.8;
};
this.disableScroll=function(v){
if(this.disableTouchScroll===v||this.domNode.style.display==="none"){
return;
}
this.disableTouchScroll=v;
this.scrollBar=!v;
dm.disableHideAddressBar=dm.disableResizeAll=v;
var of=v?"visible":"hidden";
_8.set(this.domNode,"overflow",of);
_8.set(_5.doc.documentElement,"overflow",of);
_8.set(_5.body(),"overflow",of);
var c=this.containerNode;
if(v){
if(!c.style.webkitTransform){
this.stopAnimation();
this.toTopLeft();
}
var mt=parseInt(c.style.marginTop)||0;
var h=c.offsetHeight+mt+this.fixedFooterHeight-this._appFooterHeight;
_8.set(this.domNode,"height",h+"px");
this._cPos={x:parseInt(c.style.left)||0,y:parseInt(c.style.top)||0};
_8.set(c,{top:"0px",left:"0px"});
var a=_5.doc.activeElement;
if(a){
var at=0;
for(var n=a;n.tagName!="BODY";n=n.offsetParent){
at+=n.offsetTop;
}
var st=at+a.clientHeight+10-this.getScreenSize().h;
if(st>0){
_5.body().scrollTop=st;
}
}
}else{
if(this._cPos){
_8.set(c,{top:this._cPos.y+"px",left:this._cPos.x+"px"});
this._cPos=null;
}
var _11=this.domNode.getElementsByTagName("*");
for(var i=0;i<_11.length;i++){
_11[i].blur&&_11[i].blur();
}
dm.resizeAll&&dm.resizeAll();
}
};
this.onScreenSizeChanged=function(e){
var sz=this.getScreenSize();
if(sz.w*sz.h>this._sz.w*this._sz.h){
this._sz=sz;
}
this.disableScroll(this.isKeyboardShown());
};
this.toTransform=function(e){
var c=this.containerNode;
if(c.offsetTop===0&&c.offsetLeft===0||!c._webkitTransform){
return;
}
_8.set(c,{webkitTransform:c._webkitTransform,top:"0px",left:"0px"});
c._webkitTransform=null;
};
this.toTopLeft=function(){
var c=this.containerNode;
if(!c.style.webkitTransform){
return;
}
c._webkitTransform=c.style.webkitTransform;
var pos=this.getPos();
_8.set(c,{webkitTransform:"",top:pos.y+"px",left:pos.x+"px"});
};
this.resize=function(e){
this._appFooterHeight=(this.fixedFooterHeight&&!this.isLocalFooter)?this.fixedFooterHeight:0;
if(this.isLocalHeader){
this.containerNode.style.marginTop=this.fixedHeaderHeight+"px";
}
var top=0;
for(var n=this.domNode;n&&n.tagName!="BODY";n=n.offsetParent){
n=this.findDisp(n);
if(!n){
break;
}
top+=n.offsetTop;
}
var h,_12=this.getScreenSize().h,dh=_12-top-this._appFooterHeight;
if(this.height==="inherit"){
if(this.domNode.offsetParent){
h=this.domNode.offsetParent.offsetHeight+"px";
}
}else{
if(this.height==="auto"){
var _13=this.domNode.offsetParent;
if(_13){
this.domNode.style.height="0px";
var _14=_13.getBoundingClientRect(),_15=this.domNode.getBoundingClientRect(),_16=_14.bottom-this._appFooterHeight;
if(_15.bottom>=_16){
dh=_12-(_15.top-_14.top)-this._appFooterHeight;
}else{
dh=_16-_15.bottom;
}
}
var _17=Math.max(this.domNode.scrollHeight,this.containerNode.scrollHeight);
h=(_17?Math.min(_17,dh):dh)+"px";
}else{
if(this.height){
h=this.height;
}
}
}
if(!h){
h=dh+"px";
}
if(h.charAt(0)!=="-"&&h!=="default"){
this.domNode.style.height=h;
}
this.onTouchEnd();
};
this.onFlickAnimationStart=function(e){
_3.stop(e);
};
this.onFlickAnimationEnd=function(e){
var an=e&&e.animationName;
if(an&&an.indexOf("scrollableViewScroll2")===-1){
if(an.indexOf("scrollableViewScroll0")!==-1){
_6.remove(this._scrollBarNodeV,"mblScrollableScrollTo0");
}else{
if(an.indexOf("scrollableViewScroll1")!==-1){
_6.remove(this._scrollBarNodeH,"mblScrollableScrollTo1");
}else{
if(this._scrollBarNodeV){
this._scrollBarNodeV.className="";
}
if(this._scrollBarNodeH){
this._scrollBarNodeH.className="";
}
}
}
return;
}
if(e&&e.srcElement){
_3.stop(e);
}
this.stopAnimation();
if(this._bounce){
var _18=this;
var _19=_18._bounce;
setTimeout(function(){
_18.slideTo(_19,0.3,"ease-out");
},0);
_18._bounce=undefined;
}else{
this.hideScrollBar();
this.removeCover();
if(this._aw){
this.toTopLeft();
}
}
};
this.isFormElement=function(_1a){
if(_1a&&_1a.nodeType!==1){
_1a=_1a.parentNode;
}
if(!_1a||_1a.nodeType!==1){
return false;
}
var t=_1a.tagName;
return (t==="SELECT"||t==="INPUT"||t==="TEXTAREA"||t==="BUTTON");
};
this.onTouchStart=function(e){
if(this.disableTouchScroll){
return;
}
if(this._conn&&(new Date()).getTime()-this.startTime<500){
return;
}
if(!this._conn){
this._conn=[];
this._conn.push(_2.connect(_5.doc,_9("touch")?"touchmove":"onmousemove",this,"onTouchMove"));
this._conn.push(_2.connect(_5.doc,_9("touch")?"touchend":"onmouseup",this,"onTouchEnd"));
}
this._aborted=false;
if(_6.contains(this.containerNode,"mblScrollableScrollTo2")){
this.abort();
}else{
if(this._scrollBarNodeV){
this._scrollBarNodeV.className="";
}
if(this._scrollBarNodeH){
this._scrollBarNodeH.className="";
}
}
if(this._aw){
this.toTransform(e);
}
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.touchStartY=e.touches?e.touches[0].pageY:e.clientY;
this.startTime=(new Date()).getTime();
this.startPos=this.getPos();
this._dim=this.getDim();
this._time=[0];
this._posX=[this.touchStartX];
this._posY=[this.touchStartY];
this._locked=false;
if(!this.isFormElement(e.target)&&!this.isNested){
_3.stop(e);
}
};
this.onTouchMove=function(e){
if(this._locked){
return;
}
var x=e.touches?e.touches[0].pageX:e.clientX;
var y=e.touches?e.touches[0].pageY:e.clientY;
var dx=x-this.touchStartX;
var dy=y-this.touchStartY;
var to={x:this.startPos.x+dx,y:this.startPos.y+dy};
var dim=this._dim;
dx=Math.abs(dx);
dy=Math.abs(dy);
if(this._time.length==1){
if(this.dirLock){
if(this._v&&!this._h&&dx>=this.threshold&&dx>=dy||(this._h||this._f)&&!this._v&&dy>=this.threshold&&dy>=dx){
this._locked=true;
return;
}
}
if(this._v&&Math.abs(dy)<this.threshold||(this._h||this._f)&&Math.abs(dx)<this.threshold){
return;
}
this.addCover();
this.showScrollBar();
}
var _1b=this.weight;
if(this._v&&this.constraint){
if(to.y>0){
to.y=Math.round(to.y*_1b);
}else{
if(to.y<-dim.o.h){
if(dim.c.h<dim.d.h){
to.y=Math.round(to.y*_1b);
}else{
to.y=-dim.o.h-Math.round((-dim.o.h-to.y)*_1b);
}
}
}
}
if((this._h||this._f)&&this.constraint){
if(to.x>0){
to.x=Math.round(to.x*_1b);
}else{
if(to.x<-dim.o.w){
if(dim.c.w<dim.d.w){
to.x=Math.round(to.x*_1b);
}else{
to.x=-dim.o.w-Math.round((-dim.o.w-to.x)*_1b);
}
}
}
}
this.scrollTo(to);
var max=10;
var n=this._time.length;
if(n>=2){
var d0,d1;
if(this._v&&!this._h){
d0=this._posY[n-1]-this._posY[n-2];
d1=y-this._posY[n-1];
}else{
if(!this._v&&this._h){
d0=this._posX[n-1]-this._posX[n-2];
d1=x-this._posX[n-1];
}
}
if(d0*d1<0){
this._time=[this._time[n-1]];
this._posX=[this._posX[n-1]];
this._posY=[this._posY[n-1]];
n=1;
}
}
if(n==max){
this._time.shift();
this._posX.shift();
this._posY.shift();
}
this._time.push((new Date()).getTime()-this.startTime);
this._posX.push(x);
this._posY.push(y);
};
this.onTouchEnd=function(e){
if(this._locked){
return;
}
var _1c=this._speed={x:0,y:0};
var dim=this._dim;
var pos=this.getPos();
var to={};
if(e){
if(!this._conn){
return;
}
for(var i=0;i<this._conn.length;i++){
_2.disconnect(this._conn[i]);
}
this._conn=null;
var n=this._time.length;
var _1d=false;
if(!this._aborted){
if(n<=1){
_1d=true;
}else{
if(n==2&&Math.abs(this._posY[1]-this._posY[0])<4&&_9("touch")){
_1d=true;
}
}
}
var _1e=this.isFormElement(e.target);
if(_1d&&!_1e){
this.hideScrollBar();
this.removeCover();
if(_9("touch")){
var _1f=e.target;
if(_1f.nodeType!=1){
_1f=_1f.parentNode;
}
var ev=_5.doc.createEvent("MouseEvents");
ev.initMouseEvent("click",true,true,_5.global,1,e.screenX,e.screenY,e.clientX,e.clientY);
setTimeout(function(){
_1f.dispatchEvent(ev);
},0);
}
return;
}else{
if(this._aw&&_1d&&_1e){
this.hideScrollBar();
this.toTopLeft();
return;
}
}
_1c=this._speed=this.getSpeed();
}else{
if(pos.x==0&&pos.y==0){
return;
}
dim=this.getDim();
}
if(this._v){
to.y=pos.y+_1c.y;
}
if(this._h||this._f){
to.x=pos.x+_1c.x;
}
this.adjustDestination(to,pos);
if(this.scrollDir=="v"&&dim.c.h<dim.d.h){
this.slideTo({y:0},0.3,"ease-out");
return;
}else{
if(this.scrollDir=="h"&&dim.c.w<dim.d.w){
this.slideTo({x:0},0.3,"ease-out");
return;
}else{
if(this._v&&this._h&&dim.c.h<dim.d.h&&dim.c.w<dim.d.w){
this.slideTo({x:0,y:0},0.3,"ease-out");
return;
}
}
}
var _20,_21="ease-out";
var _22={};
if(this._v&&this.constraint){
if(to.y>0){
if(pos.y>0){
_20=0.3;
to.y=0;
}else{
to.y=Math.min(to.y,20);
_21="linear";
_22.y=0;
}
}else{
if(-_1c.y>dim.o.h-(-pos.y)){
if(pos.y<-dim.o.h){
_20=0.3;
to.y=dim.c.h<=dim.d.h?0:-dim.o.h;
}else{
to.y=Math.max(to.y,-dim.o.h-20);
_21="linear";
_22.y=-dim.o.h;
}
}
}
}
if((this._h||this._f)&&this.constraint){
if(to.x>0){
if(pos.x>0){
_20=0.3;
to.x=0;
}else{
to.x=Math.min(to.x,20);
_21="linear";
_22.x=0;
}
}else{
if(-_1c.x>dim.o.w-(-pos.x)){
if(pos.x<-dim.o.w){
_20=0.3;
to.x=dim.c.w<=dim.d.w?0:-dim.o.w;
}else{
to.x=Math.max(to.x,-dim.o.w-20);
_21="linear";
_22.x=-dim.o.w;
}
}
}
}
this._bounce=(_22.x!==undefined||_22.y!==undefined)?_22:undefined;
if(_20===undefined){
var _23,_24;
if(this._v&&this._h){
_24=Math.sqrt(_1c.x+_1c.x+_1c.y*_1c.y);
_23=Math.sqrt(Math.pow(to.y-pos.y,2)+Math.pow(to.x-pos.x,2));
}else{
if(this._v){
_24=_1c.y;
_23=to.y-pos.y;
}else{
if(this._h){
_24=_1c.x;
_23=to.x-pos.x;
}
}
}
if(_23===0&&!e){
return;
}
_20=_24!==0?Math.abs(_23/_24):0.01;
}
this.slideTo(to,_20,_21);
};
this.adjustDestination=function(to,pos){
};
this.abort=function(){
this.scrollTo(this.getPos());
this.stopAnimation();
this._aborted=true;
};
this.stopAnimation=function(){
_6.remove(this.containerNode,"mblScrollableScrollTo2");
if(_9("android")){
_8.set(this.containerNode,"webkitAnimationDuration","0s");
}
if(this._scrollBarV){
this._scrollBarV.className="";
}
if(this._scrollBarH){
this._scrollBarH.className="";
}
};
this.getSpeed=function(){
var x=0,y=0,n=this._time.length;
if(n>=2&&(new Date()).getTime()-this.startTime-this._time[n-1]<500){
var dy=this._posY[n-(n>3?2:1)]-this._posY[(n-6)>=0?n-6:0];
var dx=this._posX[n-(n>3?2:1)]-this._posX[(n-6)>=0?n-6:0];
var dt=this._time[n-(n>3?2:1)]-this._time[(n-6)>=0?n-6:0];
y=this.calcSpeed(dy,dt);
x=this.calcSpeed(dx,dt);
}
return {x:x,y:y};
};
this.calcSpeed=function(d,t){
return Math.round(d/t*100)*4;
};
this.scrollTo=function(to,_25,_26){
var s=(_26||this.containerNode).style;
if(_9("webkit")){
s.webkitTransform=this.makeTranslateStr(to);
}else{
if(this._v){
s.top=to.y+"px";
}
if(this._h||this._f){
s.left=to.x+"px";
}
}
if(!_25){
this.scrollScrollBarTo(this.calcScrollBarPos(to));
}
};
this.slideTo=function(to,_27,_28){
this._runSlideAnimation(this.getPos(),to,_27,_28,this.containerNode,2);
this.slideScrollBarTo(to,_27,_28);
};
this.makeTranslateStr=function(to){
var y=this._v&&typeof to.y=="number"?to.y+"px":"0px";
var x=(this._h||this._f)&&typeof to.x=="number"?to.x+"px":"0px";
return dm.hasTranslate3d?"translate3d("+x+","+y+",0px)":"translate("+x+","+y+")";
};
this.getPos=function(){
if(_9("webkit")){
var m=_5.doc.defaultView.getComputedStyle(this.containerNode,"")["-webkit-transform"];
if(m&&m.indexOf("matrix")===0){
var arr=m.split(/[,\s\)]+/);
return {y:arr[5]-0,x:arr[4]-0};
}
return {x:0,y:0};
}else{
var y=parseInt(this.containerNode.style.top)||0;
return {y:y,x:this.containerNode.offsetLeft};
}
};
this.getDim=function(){
var d={};
d.c={h:this.containerNode.offsetHeight,w:this.containerNode.offsetWidth};
d.v={h:this.domNode.offsetHeight+this._appFooterHeight,w:this.domNode.offsetWidth};
d.d={h:d.v.h-this.fixedHeaderHeight-this.fixedFooterHeight,w:d.v.w};
d.o={h:d.c.h-d.v.h+this.fixedHeaderHeight+this.fixedFooterHeight,w:d.c.w-d.v.w};
return d;
};
this.showScrollBar=function(){
if(!this.scrollBar){
return;
}
var dim=this._dim;
if(this.scrollDir=="v"&&dim.c.h<=dim.d.h){
return;
}
if(this.scrollDir=="h"&&dim.c.w<=dim.d.w){
return;
}
if(this._v&&this._h&&dim.c.h<=dim.d.h&&dim.c.w<=dim.d.w){
return;
}
var _29=function(_2a,dir){
var bar=_2a["_scrollBarNode"+dir];
if(!bar){
var _2b=_7.create("div",null,_2a.domNode);
var _2c={position:"absolute",overflow:"hidden"};
if(dir=="V"){
_2c.right="2px";
_2c.width="5px";
}else{
_2c.bottom=(_2a.isLocalFooter?_2a.fixedFooterHeight:0)+2+"px";
_2c.height="5px";
}
_8.set(_2b,_2c);
_2b.className="mblScrollBarWrapper";
_2a["_scrollBarWrapper"+dir]=_2b;
bar=_7.create("div",null,_2b);
_8.set(bar,{opacity:0.6,position:"absolute",backgroundColor:"#606060",fontSize:"1px",webkitBorderRadius:"2px",MozBorderRadius:"2px",webkitTransformOrigin:"0 0",zIndex:2147483647});
_8.set(bar,dir=="V"?{width:"5px"}:{height:"5px"});
_2a["_scrollBarNode"+dir]=bar;
}
return bar;
};
if(this._v&&!this._scrollBarV){
this._scrollBarV=_29(this,"V");
}
if(this._h&&!this._scrollBarH){
this._scrollBarH=_29(this,"H");
}
this.resetScrollBar();
};
this.hideScrollBar=function(){
var _2d;
if(this.fadeScrollBar&&_9("webkit")){
if(!dm._fadeRule){
var _2e=_7.create("style",null,_5.doc.getElementsByTagName("head")[0]);
_2e.textContent=".mblScrollableFadeScrollBar{"+"  -webkit-animation-duration: 1s;"+"  -webkit-animation-name: scrollableViewFadeScrollBar;}"+"@-webkit-keyframes scrollableViewFadeScrollBar{"+"  from { opacity: 0.6; }"+"  to { opacity: 0; }}";
dm._fadeRule=_2e.sheet.cssRules[1];
}
_2d=dm._fadeRule;
}
if(!this.scrollBar){
return;
}
var f=function(bar,_2f){
_8.set(bar,{opacity:0,webkitAnimationDuration:""});
if(_2f._aw){
bar.style.webkitTransform="";
}else{
bar.className="mblScrollableFadeScrollBar";
}
};
if(this._scrollBarV){
f(this._scrollBarV,this);
this._scrollBarV=null;
}
if(this._scrollBarH){
f(this._scrollBarH,this);
this._scrollBarH=null;
}
};
this.calcScrollBarPos=function(to){
var pos={};
var dim=this._dim;
var f=function(_30,_31,t,d,c){
var y=Math.round((d-_31-8)/(d-c)*t);
if(y<-_31+5){
y=-_31+5;
}
if(y>_30-5){
y=_30-5;
}
return y;
};
if(typeof to.y=="number"&&this._scrollBarV){
pos.y=f(this._scrollBarWrapperV.offsetHeight,this._scrollBarV.offsetHeight,to.y,dim.d.h,dim.c.h);
}
if(typeof to.x=="number"&&this._scrollBarH){
pos.x=f(this._scrollBarWrapperH.offsetWidth,this._scrollBarH.offsetWidth,to.x,dim.d.w,dim.c.w);
}
return pos;
};
this.scrollScrollBarTo=function(to){
if(!this.scrollBar){
return;
}
if(this._v&&this._scrollBarV&&typeof to.y=="number"){
if(_9("webkit")){
this._scrollBarV.style.webkitTransform=this.makeTranslateStr({y:to.y});
}else{
this._scrollBarV.style.top=to.y+"px";
}
}
if(this._h&&this._scrollBarH&&typeof to.x=="number"){
if(_9("webkit")){
this._scrollBarH.style.webkitTransform=this.makeTranslateStr({x:to.x});
}else{
this._scrollBarH.style.left=to.x+"px";
}
}
};
this.slideScrollBarTo=function(to,_32,_33){
if(!this.scrollBar){
return;
}
var _34=this.calcScrollBarPos(this.getPos());
var _35=this.calcScrollBarPos(to);
if(this._v&&this._scrollBarV){
this._runSlideAnimation({y:_34.y},{y:_35.y},_32,_33,this._scrollBarV,0);
}
if(this._h&&this._scrollBarH){
this._runSlideAnimation({x:_34.x},{x:_35.x},_32,_33,this._scrollBarH,1);
}
};
this._runSlideAnimation=function(_36,to,_37,_38,_39,idx){
if(_9("webkit")){
this.setKeyframes(_36,to,idx);
_8.set(_39,{webkitAnimationDuration:_37+"s",webkitAnimationTimingFunction:_38});
_6.add(_39,"mblScrollableScrollTo"+idx);
if(idx==2){
this.scrollTo(to,true,_39);
}else{
this.scrollScrollBarTo(to);
}
}else{
if(_b.fx&&_b.fx.easing&&_37){
var s=_b.fx.slideTo({node:_39,duration:_37*1000,left:to.x,top:to.y,easing:(_38=="ease-out")?_b.fx.easing.quadOut:_b.fx.easing.linear}).play();
if(idx==2){
_2.connect(s,"onEnd",this,"onFlickAnimationEnd");
}
}else{
if(idx==2){
this.scrollTo(to,false,_39);
this.onFlickAnimationEnd();
}else{
this.scrollScrollBarTo(to);
}
}
}
};
this.resetScrollBar=function(){
var f=function(_3a,bar,d,c,hd,v){
if(!bar){
return;
}
var _3b={};
_3b[v?"top":"left"]=hd+4+"px";
var t=(d-8)<=0?1:d-8;
_3b[v?"height":"width"]=t+"px";
_8.set(_3a,_3b);
var l=Math.round(d*d/c);
l=Math.min(Math.max(l-8,5),t);
bar.style[v?"height":"width"]=l+"px";
_8.set(bar,{"opacity":0.6});
};
var dim=this.getDim();
f(this._scrollBarWrapperV,this._scrollBarV,dim.d.h,dim.c.h,this.fixedHeaderHeight,true);
f(this._scrollBarWrapperH,this._scrollBarH,dim.d.w,dim.c.w,0);
this.createMask();
};
this.createMask=function(){
if(!_9("webkit")){
return;
}
var ctx;
if(this._scrollBarWrapperV){
var h=this._scrollBarWrapperV.offsetHeight;
ctx=_5.doc.getCSSCanvasContext("2d","scrollBarMaskV",5,h);
ctx.fillStyle="rgba(0,0,0,0.5)";
ctx.fillRect(1,0,3,2);
ctx.fillRect(0,1,5,1);
ctx.fillRect(0,h-2,5,1);
ctx.fillRect(1,h-1,3,2);
ctx.fillStyle="rgb(0,0,0)";
ctx.fillRect(0,2,5,h-4);
this._scrollBarWrapperV.style.webkitMaskImage="-webkit-canvas(scrollBarMaskV)";
}
if(this._scrollBarWrapperH){
var w=this._scrollBarWrapperH.offsetWidth;
ctx=_5.doc.getCSSCanvasContext("2d","scrollBarMaskH",w,5);
ctx.fillStyle="rgba(0,0,0,0.5)";
ctx.fillRect(0,1,2,3);
ctx.fillRect(1,0,1,5);
ctx.fillRect(w-2,0,1,5);
ctx.fillRect(w-1,1,2,3);
ctx.fillStyle="rgb(0,0,0)";
ctx.fillRect(2,0,w-4,5);
this._scrollBarWrapperH.style.webkitMaskImage="-webkit-canvas(scrollBarMaskH)";
}
};
this.flashScrollBar=function(){
if(this.disableFlashScrollBar||!this.domNode){
return;
}
this._dim=this.getDim();
if(this._dim.d.h<=0){
return;
}
this.showScrollBar();
var _3c=this;
setTimeout(function(){
_3c.hideScrollBar();
},300);
};
this.addCover=function(){
if(!_9("touch")&&!this.noCover){
if(!this._cover){
this._cover=_7.create("div",null,_5.doc.body);
_8.set(this._cover,{backgroundColor:"#ffff00",opacity:0,position:"absolute",top:"0px",left:"0px",width:"100%",height:"100%",zIndex:2147483647});
this._ch.push(_2.connect(this._cover,_9("touch")?"touchstart":"onmousedown",this,"onTouchEnd"));
}else{
this._cover.style.display="";
}
this.setSelectable(this._cover,false);
this.setSelectable(this.domNode,false);
}
};
this.removeCover=function(){
if(!_9("touch")&&this._cover){
this._cover.style.display="none";
this.setSelectable(this._cover,true);
this.setSelectable(this.domNode,true);
}
};
this.setKeyframes=function(_3d,to,idx){
if(!dm._rule){
dm._rule=[];
}
if(!dm._rule[idx]){
var _3e=_7.create("style",null,_5.doc.getElementsByTagName("head")[0]);
_3e.textContent=".mblScrollableScrollTo"+idx+"{-webkit-animation-name: scrollableViewScroll"+idx+";}"+"@-webkit-keyframes scrollableViewScroll"+idx+"{}";
dm._rule[idx]=_3e.sheet.cssRules[1];
}
var _3f=dm._rule[idx];
if(_3f){
if(_3d){
_3f.deleteRule("from");
_3f.insertRule("from { -webkit-transform: "+this.makeTranslateStr(_3d)+"; }");
}
if(to){
if(to.x===undefined){
to.x=_3d.x;
}
if(to.y===undefined){
to.y=_3d.y;
}
_3f.deleteRule("to");
_3f.insertRule("to { -webkit-transform: "+this.makeTranslateStr(to)+"; }");
}
}
};
this.setSelectable=function(_40,_41){
_40.style.KhtmlUserSelect=_41?"auto":"none";
_40.style.MozUserSelect=_41?"":"none";
_40.onselectstart=_41?null:function(){
return false;
};
if(_9("ie")){
_40.unselectable=_41?"":"on";
var _42=_40.getElementsByTagName("*");
for(var i=0;i<_42.length;i++){
_42[i].unselectable=_41?"":"on";
}
}
};
if(_9("webkit")){
var _43=_5.doc.createElement("div");
_43.style.webkitTransform="translate3d(0px,1px,0px)";
_5.doc.documentElement.appendChild(_43);
var v=_5.doc.defaultView.getComputedStyle(_43,"")["-webkit-transform"];
dm.hasTranslate3d=v&&v.indexOf("matrix")===0;
_5.doc.documentElement.removeChild(_43);
}
};
dm.scrollable=_a;
return _a;
});
