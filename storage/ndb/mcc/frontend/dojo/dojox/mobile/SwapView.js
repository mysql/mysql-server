//>>built
define("dojox/mobile/SwapView",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-class","dijit/registry","./View","./_ScrollableMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _3("dojox.mobile.SwapView",[_7,_8],{scrollDir:"f",weight:1.2,buildRendering:function(){
this.inherited(arguments);
_5.add(this.domNode,"mblSwapView");
this.setSelectable(this.domNode,false);
this.containerNode=this.domNode;
_2.subscribe("/dojox/mobile/nextPage",this,"handleNextPage");
_2.subscribe("/dojox/mobile/prevPage",this,"handlePrevPage");
this.findAppBars();
},resize:function(){
this.inherited(arguments);
_1.forEach(this.getChildren(),function(_9){
if(_9.resize){
_9.resize();
}
});
},onTouchStart:function(e){
var _a=this.domNode.offsetTop;
var _b=this.nextView(this.domNode);
if(_b){
_b.stopAnimation();
_5.add(_b.domNode,"mblIn");
_b.containerNode.style.paddingTop=_a+"px";
}
var _c=this.previousView(this.domNode);
if(_c){
_c.stopAnimation();
_5.add(_c.domNode,"mblIn");
_c.containerNode.style.paddingTop=_a+"px";
}
this.inherited(arguments);
},handleNextPage:function(w){
var _d=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_d.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(1);
},handlePrevPage:function(w){
var _e=w.refId&&_4.byId(w.refId)||w.domNode;
if(this.domNode.parentNode!==_e.parentNode){
return;
}
if(this.getShowingView()!==this){
return;
}
this.goTo(-1);
},goTo:function(_f){
var w=this.domNode.offsetWidth;
var _10=(_f==1)?this.nextView(this.domNode):this.previousView(this.domNode);
if(!_10){
return;
}
_10._beingFlipped=true;
_10.scrollTo({x:w*_f});
_10._beingFlipped=false;
_10.domNode.style.display="";
_5.add(_10.domNode,"mblIn");
this.slideTo({x:0},0.5,"ease-out",{x:-w*_f});
},isSwapView:function(_11){
return (_11&&_11.nodeType===1&&_5.contains(_11,"mblSwapView"));
},nextView:function(_12){
for(var n=_12.nextSibling;n;n=n.nextSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},previousView:function(_13){
for(var n=_13.previousSibling;n;n=n.previousSibling){
if(this.isSwapView(n)){
return _6.byNode(n);
}
}
return null;
},scrollTo:function(to){
if(!this._beingFlipped){
var _14,x;
if(to.x<0){
_14=this.nextView(this.domNode);
x=to.x+this.domNode.offsetWidth;
}else{
_14=this.previousView(this.domNode);
x=to.x-this.domNode.offsetWidth;
}
if(_14){
_14.domNode.style.display="";
_14._beingFlipped=true;
_14.scrollTo({x:x});
_14._beingFlipped=false;
}
}
this.inherited(arguments);
},slideTo:function(to,_15,_16,_17){
if(!this._beingFlipped){
var w=this.domNode.offsetWidth;
var pos=_17||this.getPos();
var _18,_19;
if(pos.x<0){
_18=this.nextView(this.domNode);
if(pos.x<-w/4){
if(_18){
to.x=-w;
_19=0;
}
}else{
if(_18){
_19=w;
}
}
}else{
_18=this.previousView(this.domNode);
if(pos.x>w/4){
if(_18){
to.x=w;
_19=0;
}
}else{
if(_18){
_19=-w;
}
}
}
if(_18){
_18._beingFlipped=true;
_18.slideTo({x:_19},_15,_16);
_18._beingFlipped=false;
if(_19===0){
dojox.mobile.currentView=_18;
}
_18.domNode._isShowing=(_18&&_19===0);
}
this.domNode._isShowing=!(_18&&_19===0);
}
this.inherited(arguments);
},onFlickAnimationEnd:function(e){
if(e&&e.animationName&&e.animationName!=="scrollableViewScroll2"){
return;
}
var _1a=this.domNode.parentNode.childNodes;
for(var i=0;i<_1a.length;i++){
var c=_1a[i];
if(this.isSwapView(c)){
_5.remove(c,"mblIn");
if(!c._isShowing){
c.style.display="none";
}
}
}
this.inherited(arguments);
if(this.getShowingView()===this){
_2.publish("/dojox/mobile/viewChanged",[this]);
this.containerNode.style.paddingTop="";
}
}});
});
