//>>built
define("dojox/mobile/SwapView",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-class","dijit/registry","./View","./_ScrollableMixin","./sniff","./_css3","dojo/has!dojo-bidi?dojox/mobile/bidi/SwapView"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_3(_9("dojo-bidi")?"dojox.mobile.NonBidiSwapView":"dojox.mobile.SwapView",[_7,_8],{scrollDir:"f",weight:1.2,_endOfTransitionTimeoutHandle:null,buildRendering:function(){
this.inherited(arguments);
_5.add(this.domNode,"mblSwapView");
this.setSelectable(this.domNode,false);
this.containerNode=this.domNode;
this.subscribe("/dojox/mobile/nextPage","handleNextPage");
this.subscribe("/dojox/mobile/prevPage","handlePrevPage");
this.noResize=true;
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
},resize:function(){
this.inherited(arguments);
_1.forEach(this.getChildren(),function(_d){
if(_d.resize){
_d.resize();
}
});
},onTouchStart:function(e){
if(this._siblingViewsInMotion()){
this.propagatable?e.preventDefault():event.stop(e);
return;
}
var _e=this.domNode.offsetTop;
var _f=this.nextView(this.domNode);
if(_f){
_f.stopAnimation();
_5.add(_f.domNode,"mblIn");
_f.containerNode.style.paddingTop=_e+"px";
}
var _10=this.previousView(this.domNode);
if(_10){
_10.stopAnimation();
_5.add(_10.domNode,"mblIn");
_10.containerNode.style.paddingTop=_e+"px";
}
this._setSiblingViewsInMotion(true);
this.inherited(arguments);
},onTouchEnd:function(e){
if(e){
if(!this._moved){
this._setSiblingViewsInMotion(false);
}else{
this._endOfTransitionTimeoutHandle=this.defer(function(){
this._setSiblingViewsInMotion(false);
},1000);
}
}
this.inherited(arguments);
},handleNextPage:function(w){
var _11=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_11.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(1);
},handlePrevPage:function(w){
var _12=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_12.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(-1);
},goTo:function(dir,_13){
var _14=_13?_6.byId(_13):((dir==1)?this.nextView(this.domNode):this.previousView(this.domNode));
if(_14&&_14!==this){
this.stopAnimation();
_14.stopAnimation();
this.domNode._isShowing=false;
_14.domNode._isShowing=true;
this.performTransition(_14.id,dir,"slide",null,function(){
_2.publish("/dojox/mobile/viewChanged",[_14]);
});
}
},isSwapView:function(_15){
return (_15&&_15.nodeType===1&&_5.contains(_15,"mblSwapView"));
},nextView:function(_16){
for(var n=_16.nextSibling;n;n=n.nextSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},previousView:function(_17){
for(var n=_17.previousSibling;n;n=n.previousSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},scrollTo:function(to){
if(!this._beingFlipped){
var _18,x;
if(to.x){
if(to.x<0){
_18=this.nextView(this.domNode);
x=to.x+this.domNode.offsetWidth;
}else{
_18=this.previousView(this.domNode);
x=to.x-this.domNode.offsetWidth;
}
}
if(_18){
if(_18.domNode.style.display==="none"){
_18.domNode.style.display="";
_18.resize();
}
_18._beingFlipped=true;
_18.scrollTo({x:x});
_18._beingFlipped=false;
}
}
this.inherited(arguments);
},findDisp:function(_19){
if(!_5.contains(_19,"mblSwapView")){
return this.inherited(arguments);
}
if(!_19.parentNode){
return null;
}
var _1a=_19.parentNode.childNodes;
for(var i=0;i<_1a.length;i++){
var n=_1a[i];
if(n.nodeType===1&&_5.contains(n,"mblSwapView")&&!_5.contains(n,"mblIn")&&n.style.display!=="none"){
return n;
}
}
return _19;
},slideTo:function(to,_1b,_1c,_1d){
if(!this._beingFlipped){
var w=this.domNode.offsetWidth;
var pos=_1d||this.getPos();
var _1e,_1f;
if(pos.x<0){
_1e=this.nextView(this.domNode);
if(pos.x<-w/4){
if(_1e){
to.x=-w;
_1f=0;
}
}else{
if(_1e){
_1f=w;
}
}
}else{
_1e=this.previousView(this.domNode);
if(pos.x>w/4){
if(_1e){
to.x=w;
_1f=0;
}
}else{
if(_1e){
_1f=-w;
}
}
}
if(_1e){
_1e._beingFlipped=true;
_1e.slideTo({x:_1f},_1b,_1c);
_1e._beingFlipped=false;
_1e.domNode._isShowing=(_1e&&_1f===0);
}
this.domNode._isShowing=!(_1e&&_1f===0);
}
this.inherited(arguments);
},onAnimationEnd:function(e){
if(e&&e.target&&_5.contains(e.target,"mblScrollableScrollTo2")){
return;
}
this.inherited(arguments);
},onFlickAnimationEnd:function(e){
if(this._endOfTransitionTimeoutHandle){
this._endOfTransitionTimeoutHandle=this._endOfTransitionTimeoutHandle.remove();
}
if(e&&e.target&&!_5.contains(e.target,"mblScrollableScrollTo2")){
return;
}
this.inherited(arguments);
if(this.domNode._isShowing){
_1.forEach(this.domNode.parentNode.childNodes,function(c){
if(this.isSwapView(c)){
_5.remove(c,"mblIn");
if(!c._isShowing){
c.style.display="none";
c.style[_a.name("transform")]="";
c.style.left="0px";
c.style.paddingTop="";
}
}
},this);
_2.publish("/dojox/mobile/viewChanged",[this]);
this.containerNode.style.paddingTop="";
}else{
if(!_9("css3-animations")){
this.containerNode.style.left="0px";
}
}
this._setSiblingViewsInMotion(false);
},_setSiblingViewsInMotion:function(_20){
var _21=_20?"true":false;
var _22=this.domNode.parentNode;
if(_22){
_22.setAttribute("data-dojox-mobile-swapview-inmotion",_21);
}
},_siblingViewsInMotion:function(){
var _23=this.domNode.parentNode;
if(_23){
return _23.getAttribute("data-dojox-mobile-swapview-inmotion")=="true";
}else{
return false;
}
}});
return _9("dojo-bidi")?_3("dojox.mobile.SwapView",[_c,_b]):_c;
});
