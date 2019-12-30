//>>built
define("dojox/mobile/SwapView",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-class","dijit/registry","./View","./_ScrollableMixin","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _3("dojox.mobile.SwapView",[_7,_8],{scrollDir:"f",weight:1.2,buildRendering:function(){
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
_1.forEach(this.getChildren(),function(_a){
if(_a.resize){
_a.resize();
}
});
},onTouchStart:function(e){
var _b=this.domNode.offsetTop;
var _c=this.nextView(this.domNode);
if(_c){
_c.stopAnimation();
_5.add(_c.domNode,"mblIn");
_c.containerNode.style.paddingTop=_b+"px";
}
var _d=this.previousView(this.domNode);
if(_d){
_d.stopAnimation();
_5.add(_d.domNode,"mblIn");
_d.containerNode.style.paddingTop=_b+"px";
}
this.inherited(arguments);
},handleNextPage:function(w){
var _e=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_e.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(1);
},handlePrevPage:function(w){
var _f=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_f.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(-1);
},goTo:function(dir,_10){
var _11=_10?_6.byId(_10):((dir==1)?this.nextView(this.domNode):this.previousView(this.domNode));
if(_11&&_11!==this){
this.stopAnimation();
_11.stopAnimation();
this.domNode._isShowing=false;
_11.domNode._isShowing=true;
this.performTransition(_11.id,dir,"slide",null,function(){
_2.publish("/dojox/mobile/viewChanged",[_11]);
});
}
},isSwapView:function(_12){
return (_12&&_12.nodeType===1&&_5.contains(_12,"mblSwapView"));
},nextView:function(_13){
for(var n=_13.nextSibling;n;n=n.nextSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},previousView:function(_14){
for(var n=_14.previousSibling;n;n=n.previousSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},scrollTo:function(to){
if(!this._beingFlipped){
var _15,x;
if(to.x<0){
_15=this.nextView(this.domNode);
x=to.x+this.domNode.offsetWidth;
}else{
_15=this.previousView(this.domNode);
x=to.x-this.domNode.offsetWidth;
}
if(_15){
if(_15.domNode.style.display==="none"){
_15.domNode.style.display="";
_15.resize();
}
_15._beingFlipped=true;
_15.scrollTo({x:x});
_15._beingFlipped=false;
}
}
this.inherited(arguments);
},findDisp:function(_16){
if(!_5.contains(_16,"mblSwapView")){
return this.inherited(arguments);
}
if(!_16.parentNode){
return null;
}
var _17=_16.parentNode.childNodes;
for(var i=0;i<_17.length;i++){
var n=_17[i];
if(n.nodeType===1&&_5.contains(n,"mblSwapView")&&!_5.contains(n,"mblIn")&&n.style.display!=="none"){
return n;
}
}
return _16;
},slideTo:function(to,_18,_19,_1a){
if(!this._beingFlipped){
var w=this.domNode.offsetWidth;
var pos=_1a||this.getPos();
var _1b,_1c;
if(pos.x<0){
_1b=this.nextView(this.domNode);
if(pos.x<-w/4){
if(_1b){
to.x=-w;
_1c=0;
}
}else{
if(_1b){
_1c=w;
}
}
}else{
_1b=this.previousView(this.domNode);
if(pos.x>w/4){
if(_1b){
to.x=w;
_1c=0;
}
}else{
if(_1b){
_1c=-w;
}
}
}
if(_1b){
_1b._beingFlipped=true;
_1b.slideTo({x:_1c},_18,_19);
_1b._beingFlipped=false;
_1b.domNode._isShowing=(_1b&&_1c===0);
}
this.domNode._isShowing=!(_1b&&_1c===0);
}
this.inherited(arguments);
},onAnimationEnd:function(e){
if(e&&e.target&&_5.contains(e.target,"mblScrollableScrollTo2")){
return;
}
this.inherited(arguments);
},onFlickAnimationEnd:function(e){
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
c.style.webkitTransform="";
c.style.left="0px";
}
}
},this);
_2.publish("/dojox/mobile/viewChanged",[this]);
this.containerNode.style.paddingTop="";
}else{
if(!_9("webkit")){
this.containerNode.style.left="0px";
}
}
}});
});
