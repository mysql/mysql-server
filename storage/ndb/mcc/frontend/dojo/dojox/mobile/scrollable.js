//>>built
define("dojox/mobile/scrollable",["dojo/_base/kernel","dojo/_base/connect","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var dm=_4.getObject("dojox.mobile",true);
_9.add("translate3d",function(){
if(_9("webkit")){
var _a=_5.doc.createElement("div");
_a.style.webkitTransform="translate3d(0px,1px,0px)";
_5.doc.documentElement.appendChild(_a);
var v=_5.doc.defaultView.getComputedStyle(_a,"")["-webkit-transform"];
var _b=v&&v.indexOf("matrix")===0;
_5.doc.documentElement.removeChild(_a);
return _b;
}
});
var _c=function(){
};
_4.extend(_c,{fixedHeaderHeight:0,fixedFooterHeight:0,isLocalFooter:false,scrollBar:true,scrollDir:"v",weight:0.6,fadeScrollBar:true,disableFlashScrollBar:false,threshold:4,constraint:true,touchNode:null,propagatable:true,dirLock:false,height:"",scrollType:0,init:function(_d){
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
this._ch.push(_2.connect(this.touchNode,_9("touch")?"ontouchstart":"onmousedown",this,"onTouchStart"));
if(_9("webkit")){
this._useTopLeft=this.scrollType?this.scrollType===2:_9("android")<3;
if(!this._useTopLeft){
this._useTransformTransition=this.scrollType?this.scrollType===3:_9("iphone")>=6;
}
if(!this._useTopLeft){
if(this._useTransformTransition){
this._ch.push(_2.connect(this.domNode,"webkitTransitionEnd",this,"onFlickAnimationEnd"));
this._ch.push(_2.connect(this.domNode,"webkitTransitionStart",this,"onFlickAnimationStart"));
}else{
this._ch.push(_2.connect(this.domNode,"webkitAnimationEnd",this,"onFlickAnimationEnd"));
this._ch.push(_2.connect(this.domNode,"webkitAnimationStart",this,"onFlickAnimationStart"));
for(var i=0;i<3;i++){
this.setKeyframes(null,null,i);
}
}
if(_9("translate3d")){
_8.set(this.containerNode,"webkitTransform","translate3d(0,0,0)");
}
}else{
this._ch.push(_2.connect(this.domNode,"webkitTransitionEnd",this,"onFlickAnimationEnd"));
this._ch.push(_2.connect(this.domNode,"webkitTransitionStart",this,"onFlickAnimationStart"));
}
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
},isTopLevel:function(){
return true;
},cleanup:function(){
if(this._ch){
for(var i=0;i<this._ch.length;i++){
_2.disconnect(this._ch[i]);
}
this._ch=null;
}
},findDisp:function(_f){
if(!_f.parentNode){
return null;
}
if(_f.nodeType===1&&_6.contains(_f,"mblSwapView")&&_f.style.display!=="none"){
return _f;
}
var _10=_f.parentNode.childNodes;
for(var i=0;i<_10.length;i++){
var n=_10[i];
if(n.nodeType===1&&_6.contains(n,"mblView")&&n.style.display!=="none"){
return n;
}
}
return _f;
},getScreenSize:function(){
return {h:_5.global.innerHeight||_5.doc.documentElement.clientHeight||_5.doc.documentElement.offsetHeight,w:_5.global.innerWidth||_5.doc.documentElement.clientWidth||_5.doc.documentElement.offsetWidth};
},resize:function(e){
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
var h,_11=this.getScreenSize().h,dh=_11-top-this._appFooterHeight;
if(this.height==="inherit"){
if(this.domNode.offsetParent){
h=this.domNode.offsetParent.offsetHeight+"px";
}
}else{
if(this.height==="auto"){
var _12=this.domNode.offsetParent;
if(_12){
this.domNode.style.height="0px";
var _13=_12.getBoundingClientRect(),_14=this.domNode.getBoundingClientRect(),_15=_13.bottom-this._appFooterHeight;
if(_14.bottom>=_15){
dh=_11-(_14.top-_13.top)-this._appFooterHeight;
}else{
dh=_15-_14.bottom;
}
}
var _16=Math.max(this.domNode.scrollHeight,this.containerNode.scrollHeight);
h=(_16?Math.min(_16,dh):dh)+"px";
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
},onFlickAnimationStart:function(e){
_3.stop(e);
},onFlickAnimationEnd:function(e){
var an=e&&e.animationName;
if(an&&an.indexOf("scrollableViewScroll2")===-1){
if(an.indexOf("scrollableViewScroll0")!==-1){
if(this._scrollBarNodeV){
_6.remove(this._scrollBarNodeV,"mblScrollableScrollTo0");
}
}else{
if(an.indexOf("scrollableViewScroll1")!==-1){
if(this._scrollBarNodeH){
_6.remove(this._scrollBarNodeH,"mblScrollableScrollTo1");
}
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
if(this._useTransformTransition||this._useTopLeft){
var n=e.target;
if(n===this._scrollBarV||n===this._scrollBarH){
var cls="mblScrollableScrollTo"+(n===this._scrollBarV?"0":"1");
if(_6.contains(n,cls)){
_6.remove(n,cls);
}else{
n.className="";
}
return;
}
}
if(e&&e.srcElement){
_3.stop(e);
}
this.stopAnimation();
if(this._bounce){
var _17=this;
var _18=_17._bounce;
setTimeout(function(){
_17.slideTo(_18,0.3,"ease-out");
},0);
_17._bounce=undefined;
}else{
this.hideScrollBar();
this.removeCover();
}
},isFormElement:function(_19){
if(_19&&_19.nodeType!==1){
_19=_19.parentNode;
}
if(!_19||_19.nodeType!==1){
return false;
}
var t=_19.tagName;
return (t==="SELECT"||t==="INPUT"||t==="TEXTAREA"||t==="BUTTON");
},onTouchStart:function(e){
if(this.disableTouchScroll){
return;
}
if(this._conn&&(new Date()).getTime()-this.startTime<500){
return;
}
if(!this._conn){
this._conn=[];
this._conn.push(_2.connect(_5.doc,_9("touch")?"ontouchmove":"onmousemove",this,"onTouchMove"));
this._conn.push(_2.connect(_5.doc,_9("touch")?"ontouchend":"onmouseup",this,"onTouchEnd"));
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
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.touchStartY=e.touches?e.touches[0].pageY:e.clientY;
this.startTime=(new Date()).getTime();
this.startPos=this.getPos();
this._dim=this.getDim();
this._time=[0];
this._posX=[this.touchStartX];
this._posY=[this.touchStartY];
this._locked=false;
if(!this.isFormElement(e.target)){
this.propagatable?e.preventDefault():_3.stop(e);
}
},onTouchMove:function(e){
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
if(this._v&&this._h){
if(dy<this.threshold&&dx<this.threshold){
return;
}
}else{
if(this._v&&dy<this.threshold||(this._h||this._f)&&dx<this.threshold){
return;
}
}
this.addCover();
this.showScrollBar();
}
var _1a=this.weight;
if(this._v&&this.constraint){
if(to.y>0){
to.y=Math.round(to.y*_1a);
}else{
if(to.y<-dim.o.h){
if(dim.c.h<dim.d.h){
to.y=Math.round(to.y*_1a);
}else{
to.y=-dim.o.h-Math.round((-dim.o.h-to.y)*_1a);
}
}
}
}
if((this._h||this._f)&&this.constraint){
if(to.x>0){
to.x=Math.round(to.x*_1a);
}else{
if(to.x<-dim.o.w){
if(dim.c.w<dim.d.w){
to.x=Math.round(to.x*_1a);
}else{
to.x=-dim.o.w-Math.round((-dim.o.w-to.x)*_1a);
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
},onTouchEnd:function(e){
if(this._locked){
return;
}
var _1b=this._speed={x:0,y:0};
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
var _1c=false;
if(!this._aborted){
if(n<=1){
_1c=true;
}else{
if(n==2&&Math.abs(this._posY[1]-this._posY[0])<4&&_9("touch")){
_1c=true;
}
}
}
if(_1c){
this.hideScrollBar();
this.removeCover();
if(_9("touch")&&!this.isFormElement(e.target)&&!(_9("android")>=4.1)){
var _1d=e.target;
if(_1d.nodeType!=1){
_1d=_1d.parentNode;
}
var ev=_5.doc.createEvent("MouseEvents");
ev.initMouseEvent("click",true,true,_5.global,1,e.screenX,e.screenY,e.clientX,e.clientY);
setTimeout(function(){
_1d.dispatchEvent(ev);
},0);
}
return;
}
_1b=this._speed=this.getSpeed();
}else{
if(pos.x==0&&pos.y==0){
return;
}
dim=this.getDim();
}
if(this._v){
to.y=pos.y+_1b.y;
}
if(this._h||this._f){
to.x=pos.x+_1b.x;
}
if(this.adjustDestination(to,pos,dim)===false){
return;
}
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
var _1e,_1f="ease-out";
var _20={};
if(this._v&&this.constraint){
if(to.y>0){
if(pos.y>0){
_1e=0.3;
to.y=0;
}else{
to.y=Math.min(to.y,20);
_1f="linear";
_20.y=0;
}
}else{
if(-_1b.y>dim.o.h-(-pos.y)){
if(pos.y<-dim.o.h){
_1e=0.3;
to.y=dim.c.h<=dim.d.h?0:-dim.o.h;
}else{
to.y=Math.max(to.y,-dim.o.h-20);
_1f="linear";
_20.y=-dim.o.h;
}
}
}
}
if((this._h||this._f)&&this.constraint){
if(to.x>0){
if(pos.x>0){
_1e=0.3;
to.x=0;
}else{
to.x=Math.min(to.x,20);
_1f="linear";
_20.x=0;
}
}else{
if(-_1b.x>dim.o.w-(-pos.x)){
if(pos.x<-dim.o.w){
_1e=0.3;
to.x=dim.c.w<=dim.d.w?0:-dim.o.w;
}else{
to.x=Math.max(to.x,-dim.o.w-20);
_1f="linear";
_20.x=-dim.o.w;
}
}
}
}
this._bounce=(_20.x!==undefined||_20.y!==undefined)?_20:undefined;
if(_1e===undefined){
var _21,_22;
if(this._v&&this._h){
_22=Math.sqrt(_1b.x*_1b.x+_1b.y*_1b.y);
_21=Math.sqrt(Math.pow(to.y-pos.y,2)+Math.pow(to.x-pos.x,2));
}else{
if(this._v){
_22=_1b.y;
_21=to.y-pos.y;
}else{
if(this._h){
_22=_1b.x;
_21=to.x-pos.x;
}
}
}
if(_21===0&&!e){
return;
}
_1e=_22!==0?Math.abs(_21/_22):0.01;
}
this.slideTo(to,_1e,_1f);
},adjustDestination:function(to,pos,dim){
return true;
},abort:function(){
this.scrollTo(this.getPos());
this.stopAnimation();
this._aborted=true;
},stopAnimation:function(){
_6.remove(this.containerNode,"mblScrollableScrollTo2");
if(this._scrollBarV){
this._scrollBarV.className="";
}
if(this._scrollBarH){
this._scrollBarH.className="";
}
if(this._useTransformTransition||this._useTopLeft){
this.containerNode.style.webkitTransition="";
if(this._scrollBarV){
this._scrollBarV.style.webkitTransition="";
}
if(this._scrollBarH){
this._scrollBarH.style.webkitTransition="";
}
}
},scrollIntoView:function(_23,_24,_25){
if(!this._v){
return;
}
var c=this.containerNode,h=this.getDim().d.h,top=0;
for(var n=_23;n!==c;n=n.offsetParent){
if(!n||n.tagName==="BODY"){
return;
}
top+=n.offsetTop;
}
var y=_24?Math.max(h-c.offsetHeight,-top):Math.min(0,h-top-_23.offsetHeight);
(_25&&typeof _25==="number")?this.slideTo({y:y},_25,"ease-out"):this.scrollTo({y:y});
},getSpeed:function(){
var x=0,y=0,n=this._time.length;
if(n>=2&&(new Date()).getTime()-this.startTime-this._time[n-1]<500){
var dy=this._posY[n-(n>3?2:1)]-this._posY[(n-6)>=0?n-6:0];
var dx=this._posX[n-(n>3?2:1)]-this._posX[(n-6)>=0?n-6:0];
var dt=this._time[n-(n>3?2:1)]-this._time[(n-6)>=0?n-6:0];
y=this.calcSpeed(dy,dt);
x=this.calcSpeed(dx,dt);
}
return {x:x,y:y};
},calcSpeed:function(_26,_27){
return Math.round(_26/_27*100)*4;
},scrollTo:function(to,_28,_29){
var s=(_29||this.containerNode).style;
if(_9("webkit")){
if(!this._useTopLeft){
if(this._useTransformTransition){
s.webkitTransition="";
}
s.webkitTransform=this.makeTranslateStr(to);
}else{
s.webkitTransition="";
if(this._v){
s.top=to.y+"px";
}
if(this._h||this._f){
s.left=to.x+"px";
}
}
}else{
if(this._v){
s.top=to.y+"px";
}
if(this._h||this._f){
s.left=to.x+"px";
}
}
if(!_28){
this.scrollScrollBarTo(this.calcScrollBarPos(to));
}
},slideTo:function(to,_2a,_2b){
this._runSlideAnimation(this.getPos(),to,_2a,_2b,this.containerNode,2);
this.slideScrollBarTo(to,_2a,_2b);
},makeTranslateStr:function(to){
var y=this._v&&typeof to.y=="number"?to.y+"px":"0px";
var x=(this._h||this._f)&&typeof to.x=="number"?to.x+"px":"0px";
return _9("translate3d")?"translate3d("+x+","+y+",0px)":"translate("+x+","+y+")";
},getPos:function(){
if(_9("webkit")){
var s=_5.doc.defaultView.getComputedStyle(this.containerNode,"");
if(!this._useTopLeft){
var m=s["-webkit-transform"];
if(m&&m.indexOf("matrix")===0){
var arr=m.split(/[,\s\)]+/);
return {y:arr[5]-0,x:arr[4]-0};
}
return {x:0,y:0};
}else{
return {x:parseInt(s.left)||0,y:parseInt(s.top)||0};
}
}else{
var y=parseInt(this.containerNode.style.top)||0;
return {y:y,x:this.containerNode.offsetLeft};
}
},getDim:function(){
var d={};
d.c={h:this.containerNode.offsetHeight,w:this.containerNode.offsetWidth};
d.v={h:this.domNode.offsetHeight+this._appFooterHeight,w:this.domNode.offsetWidth};
d.d={h:d.v.h-this.fixedHeaderHeight-this.fixedFooterHeight,w:d.v.w};
d.o={h:d.c.h-d.v.h+this.fixedHeaderHeight+this.fixedFooterHeight,w:d.c.w-d.v.w};
return d;
},showScrollBar:function(){
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
var _2c=function(_2d,dir){
var bar=_2d["_scrollBarNode"+dir];
if(!bar){
var _2e=_7.create("div",null,_2d.domNode);
var _2f={position:"absolute",overflow:"hidden"};
if(dir=="V"){
_2f.right="2px";
_2f.width="5px";
}else{
_2f.bottom=(_2d.isLocalFooter?_2d.fixedFooterHeight:0)+2+"px";
_2f.height="5px";
}
_8.set(_2e,_2f);
_2e.className="mblScrollBarWrapper";
_2d["_scrollBarWrapper"+dir]=_2e;
bar=_7.create("div",null,_2e);
_8.set(bar,{opacity:0.6,position:"absolute",backgroundColor:"#606060",fontSize:"1px",webkitBorderRadius:"2px",MozBorderRadius:"2px",webkitTransformOrigin:"0 0",zIndex:2147483647});
_8.set(bar,dir=="V"?{width:"5px"}:{height:"5px"});
_2d["_scrollBarNode"+dir]=bar;
}
return bar;
};
if(this._v&&!this._scrollBarV){
this._scrollBarV=_2c(this,"V");
}
if(this._h&&!this._scrollBarH){
this._scrollBarH=_2c(this,"H");
}
this.resetScrollBar();
},hideScrollBar:function(){
if(this.fadeScrollBar&&_9("webkit")){
if(!dm._fadeRule){
var _30=_7.create("style",null,_5.doc.getElementsByTagName("head")[0]);
_30.textContent=".mblScrollableFadeScrollBar{"+"  -webkit-animation-duration: 1s;"+"  -webkit-animation-name: scrollableViewFadeScrollBar;}"+"@-webkit-keyframes scrollableViewFadeScrollBar{"+"  from { opacity: 0.6; }"+"  to { opacity: 0; }}";
dm._fadeRule=_30.sheet.cssRules[1];
}
}
if(!this.scrollBar){
return;
}
var f=function(bar,_31){
_8.set(bar,{opacity:0,webkitAnimationDuration:""});
if(!(_31._useTopLeft&&_9("android"))){
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
},calcScrollBarPos:function(to){
var pos={};
var dim=this._dim;
var f=function(_32,_33,t,d,c){
var y=Math.round((d-_33-8)/(d-c)*t);
if(y<-_33+5){
y=-_33+5;
}
if(y>_32-5){
y=_32-5;
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
},scrollScrollBarTo:function(to){
if(!this.scrollBar){
return;
}
if(this._v&&this._scrollBarV&&typeof to.y=="number"){
if(_9("webkit")){
if(!this._useTopLeft){
if(this._useTransformTransition){
this._scrollBarV.style.webkitTransition="";
}
this._scrollBarV.style.webkitTransform=this.makeTranslateStr({y:to.y});
}else{
_8.set(this._scrollBarV,{webkitTransition:"",top:to.y+"px"});
}
}else{
this._scrollBarV.style.top=to.y+"px";
}
}
if(this._h&&this._scrollBarH&&typeof to.x=="number"){
if(_9("webkit")){
if(!this._useTopLeft){
if(this._useTransformTransition){
this._scrollBarH.style.webkitTransition="";
}
this._scrollBarH.style.webkitTransform=this.makeTranslateStr({x:to.x});
}else{
_8.set(this._scrollBarH,{webkitTransition:"",left:to.x+"px"});
}
}else{
this._scrollBarH.style.left=to.x+"px";
}
}
},slideScrollBarTo:function(to,_34,_35){
if(!this.scrollBar){
return;
}
var _36=this.calcScrollBarPos(this.getPos());
var _37=this.calcScrollBarPos(to);
if(this._v&&this._scrollBarV){
this._runSlideAnimation({y:_36.y},{y:_37.y},_34,_35,this._scrollBarV,0);
}
if(this._h&&this._scrollBarH){
this._runSlideAnimation({x:_36.x},{x:_37.x},_34,_35,this._scrollBarH,1);
}
},_runSlideAnimation:function(_38,to,_39,_3a,_3b,idx){
if(_9("webkit")){
if(!this._useTopLeft){
if(this._useTransformTransition){
if(to.x===undefined){
to.x=_38.x;
}
if(to.y===undefined){
to.y=_38.y;
}
if(to.x!==_38.x||to.y!==_38.y){
_8.set(_3b,{webkitTransitionProperty:"-webkit-transform",webkitTransitionDuration:_39+"s",webkitTransitionTimingFunction:_3a});
var t=this.makeTranslateStr(to);
setTimeout(function(){
_8.set(_3b,{webkitTransform:t});
},0);
_6.add(_3b,"mblScrollableScrollTo"+idx);
}else{
this.hideScrollBar();
this.removeCover();
}
}else{
this.setKeyframes(_38,to,idx);
_8.set(_3b,{webkitAnimationDuration:_39+"s",webkitAnimationTimingFunction:_3a});
_6.add(_3b,"mblScrollableScrollTo"+idx);
if(idx==2){
this.scrollTo(to,true,_3b);
}else{
this.scrollScrollBarTo(to);
}
}
}else{
_8.set(_3b,{webkitTransitionProperty:"top, left",webkitTransitionDuration:_39+"s",webkitTransitionTimingFunction:_3a});
setTimeout(function(){
_8.set(_3b,{top:(to.y||0)+"px",left:(to.x||0)+"px"});
},0);
_6.add(_3b,"mblScrollableScrollTo"+idx);
}
}else{
if(_1.fx&&_1.fx.easing&&_39){
var s=_1.fx.slideTo({node:_3b,duration:_39*1000,left:to.x,top:to.y,easing:(_3a=="ease-out")?_1.fx.easing.quadOut:_1.fx.easing.linear}).play();
if(idx==2){
_2.connect(s,"onEnd",this,"onFlickAnimationEnd");
}
}else{
if(idx==2){
this.scrollTo(to,false,_3b);
this.onFlickAnimationEnd();
}else{
this.scrollScrollBarTo(to);
}
}
}
},resetScrollBar:function(){
var f=function(_3c,bar,d,c,hd,v){
if(!bar){
return;
}
var _3d={};
_3d[v?"top":"left"]=hd+4+"px";
var t=(d-8)<=0?1:d-8;
_3d[v?"height":"width"]=t+"px";
_8.set(_3c,_3d);
var l=Math.round(d*d/c);
l=Math.min(Math.max(l-8,5),t);
bar.style[v?"height":"width"]=l+"px";
_8.set(bar,{"opacity":0.6});
};
var dim=this.getDim();
f(this._scrollBarWrapperV,this._scrollBarV,dim.d.h,dim.c.h,this.fixedHeaderHeight,true);
f(this._scrollBarWrapperH,this._scrollBarH,dim.d.w,dim.c.w,0);
this.createMask();
},createMask:function(){
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
},flashScrollBar:function(){
if(this.disableFlashScrollBar||!this.domNode){
return;
}
this._dim=this.getDim();
if(this._dim.d.h<=0){
return;
}
this.showScrollBar();
var _3e=this;
setTimeout(function(){
_3e.hideScrollBar();
},300);
},addCover:function(){
if(!_9("touch")&&!this.noCover){
if(!dm._cover){
dm._cover=_7.create("div",null,_5.doc.body);
dm._cover.className="mblScrollableCover";
_8.set(dm._cover,{backgroundColor:"#ffff00",opacity:0,position:"absolute",top:"0px",left:"0px",width:"100%",height:"100%",zIndex:2147483647});
this._ch.push(_2.connect(dm._cover,_9("touch")?"ontouchstart":"onmousedown",this,"onTouchEnd"));
}else{
dm._cover.style.display="";
}
this.setSelectable(dm._cover,false);
this.setSelectable(this.domNode,false);
}
},removeCover:function(){
if(!_9("touch")&&dm._cover){
dm._cover.style.display="none";
this.setSelectable(dm._cover,true);
this.setSelectable(this.domNode,true);
}
},setKeyframes:function(_3f,to,idx){
if(!dm._rule){
dm._rule=[];
}
if(!dm._rule[idx]){
var _40=_7.create("style",null,_5.doc.getElementsByTagName("head")[0]);
_40.textContent=".mblScrollableScrollTo"+idx+"{-webkit-animation-name: scrollableViewScroll"+idx+";}"+"@-webkit-keyframes scrollableViewScroll"+idx+"{}";
dm._rule[idx]=_40.sheet.cssRules[1];
}
var _41=dm._rule[idx];
if(_41){
if(_3f){
_41.deleteRule("from");
_41.insertRule("from { -webkit-transform: "+this.makeTranslateStr(_3f)+"; }");
}
if(to){
if(to.x===undefined){
to.x=_3f.x;
}
if(to.y===undefined){
to.y=_3f.y;
}
_41.deleteRule("to");
_41.insertRule("to { -webkit-transform: "+this.makeTranslateStr(to)+"; }");
}
}
},setSelectable:function(_42,_43){
_42.style.KhtmlUserSelect=_43?"auto":"none";
_42.style.MozUserSelect=_43?"":"none";
_42.onselectstart=_43?null:function(){
return false;
};
if(_9("ie")){
_42.unselectable=_43?"":"on";
var _44=_42.getElementsByTagName("*");
for(var i=0;i<_44.length;i++){
_44[i].unselectable=_43?"":"on";
}
}
}});
_4.setObject("dojox.mobile.scrollable",_c);
return _c;
});
