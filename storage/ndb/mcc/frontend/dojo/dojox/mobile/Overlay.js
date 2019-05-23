//>>built
define("dojox/mobile/Overlay",["dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/window","dijit/_WidgetBase","dojo/_base/array","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dojox.mobile.Overlay",_9,{baseClass:"mblOverlay mblOverlayHidden",_reposition:function(){
var _c=_6.position(this.domNode);
var vp=_8.getBox();
if((_c.y+_c.h)!=vp.h||(_7.get(this.domNode,"position")!="absolute"&&_3("android")<3)){
_c.y=vp.t+vp.h-_c.h;
_7.set(this.domNode,{position:"absolute",top:_c.y+"px",bottom:"auto"});
}
return _c;
},show:function(_d){
_a.forEach(_b.findWidgets(this.domNode),function(w){
if(w&&w.height=="auto"&&typeof w.resize=="function"){
w.resize();
}
});
var _e=this._reposition();
if(_d){
var _f=_6.position(_d);
if(_e.y<_f.y){
_4.global.scrollBy(0,_f.y+_f.h-_e.y);
this._reposition();
}
}
var _10=this.domNode;
_5.replace(_10,["mblCoverv","mblIn"],["mblOverlayHidden","mblRevealv","mblOut","mblReverse","mblTransition"]);
setTimeout(_2.hitch(this,function(){
var _11=this.connect(_10,"webkitTransitionEnd",function(){
this.disconnect(_11);
_5.remove(_10,["mblCoverv","mblIn","mblTransition"]);
this._reposition();
});
_5.add(_10,"mblTransition");
}),100);
var _12=false;
this._moveHandle=this.connect(_4.doc.documentElement,_3("touch")?"ontouchmove":"onmousemove",function(){
_12=true;
});
this._repositionTimer=setInterval(_2.hitch(this,function(){
if(_12){
_12=false;
return;
}
this._reposition();
}),50);
return _e;
},hide:function(){
var _13=this.domNode;
if(this._moveHandle){
this.disconnect(this._moveHandle);
this._moveHandle=null;
clearInterval(this._repositionTimer);
this._repositionTimer=null;
}
if(_3("webkit")){
_5.replace(_13,["mblRevealv","mblOut","mblReverse"],["mblCoverv","mblIn","mblOverlayHidden","mblTransition"]);
setTimeout(_2.hitch(this,function(){
var _14=this.connect(_13,"webkitTransitionEnd",function(){
this.disconnect(_14);
_5.replace(_13,["mblOverlayHidden"],["mblRevealv","mblOut","mblReverse","mblTransition"]);
});
_5.add(_13,"mblTransition");
}),100);
}else{
_5.replace(_13,["mblOverlayHidden"],["mblCoverv","mblIn","mblRevealv","mblOut","mblReverse"]);
}
},onBlur:function(e){
return false;
}});
});
