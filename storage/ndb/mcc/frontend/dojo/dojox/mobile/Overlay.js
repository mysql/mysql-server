//>>built
define("dojox/mobile/Overlay",["dojo/_base/declare","dojo/_base/lang","dojo/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/window","dijit/_WidgetBase","dojo/_base/array","dijit/registry","dojo/touch","./viewRegistry","./_css3"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
return _1("dojox.mobile.Overlay",_9,{baseClass:"mblOverlay mblOverlayHidden",buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
},_reposition:function(){
var _f=_6.position(this.domNode);
var vp=_8.getBox();
var _10=_d.getEnclosingScrollable(this.domNode);
if(_10){
vp.t-=_10.getPos().y;
}
if((_f.y+_f.h)!=vp.h||(_7.get(this.domNode,"position")!="absolute"&&_3("android")<3)){
_f.y=vp.t+vp.h-_f.h;
_7.set(this.domNode,{position:"absolute",top:_f.y+"px",bottom:"auto"});
}
return _f;
},show:function(_11){
_a.forEach(_b.findWidgets(this.domNode),function(w){
if(w&&w.height=="auto"&&typeof w.resize=="function"){
w.resize();
}
});
var _12=this._reposition();
if(_11){
var _13=_6.position(_11);
if(_12.y<_13.y){
_4.global.scrollBy(0,_13.y+_13.h-_12.y);
this._reposition();
}
}
var _14=this.domNode;
_5.replace(_14,["mblCoverv","mblIn"],["mblOverlayHidden","mblRevealv","mblOut","mblReverse","mblTransition"]);
this.defer(function(){
var _15=this.connect(_14,_e.name("transitionEnd"),function(){
this.disconnect(_15);
_5.remove(_14,["mblCoverv","mblIn","mblTransition"]);
this._reposition();
});
_5.add(_14,"mblTransition");
},100);
var _16=false;
this._moveHandle=this.connect(_4.doc.documentElement,_c.move,function(){
_16=true;
});
this._repositionTimer=setInterval(_2.hitch(this,function(){
if(_16){
_16=false;
return;
}
this._reposition();
}),50);
return _12;
},hide:function(){
var _17=this.domNode;
if(this._moveHandle){
this.disconnect(this._moveHandle);
this._moveHandle=null;
clearInterval(this._repositionTimer);
this._repositionTimer=null;
}
if(_3("css3-animations")){
_5.replace(_17,["mblRevealv","mblOut","mblReverse"],["mblCoverv","mblIn","mblOverlayHidden","mblTransition"]);
this.defer(function(){
var _18=this.connect(_17,_e.name("transitionEnd"),function(){
this.disconnect(_18);
_5.replace(_17,["mblOverlayHidden"],["mblRevealv","mblOut","mblReverse","mblTransition"]);
});
_5.add(_17,"mblTransition");
},100);
}else{
_5.replace(_17,["mblOverlayHidden"],["mblCoverv","mblIn","mblRevealv","mblOut","mblReverse"]);
}
},onBlur:function(e){
return false;
}});
});
