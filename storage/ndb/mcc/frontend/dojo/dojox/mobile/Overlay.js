//>>built
define("dojox/mobile/Overlay",["dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/window","dijit/_WidgetBase","dojo/_base/array","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dojox.mobile.Overlay",_9,{baseClass:"mblOverlay mblOverlayHidden",show:function(_c){
_a.forEach(_b.findWidgets(this.domNode),function(w){
if(w&&w.height=="auto"&&typeof w.resize=="function"){
w.resize();
}
});
var vp,_d;
var _e=_2.hitch(this,function(){
_7.set(this.domNode,{position:"",top:"auto",bottom:"0px"});
_d=_6.position(this.domNode);
vp=_8.getBox();
if((_d.y+_d.h)!=vp.h||_3("android")<3){
_d.y=vp.t+vp.h-_d.h;
_7.set(this.domNode,{position:"absolute",top:_d.y+"px",bottom:"auto"});
}
});
_e();
if(_c){
var _f=_6.position(_c);
if(_d.y<_f.y){
_4.global.scrollBy(0,_f.y+_f.h-_d.y);
_e();
}
}
_5.replace(this.domNode,["mblCoverv","mblIn"],["mblOverlayHidden","mblRevealv","mblOut","mblReverse"]);
var _10=this.domNode;
setTimeout(function(){
_5.add(_10,"mblTransition");
},100);
var _11=null;
this._moveHandle=this.connect(_4.doc.documentElement,"ontouchmove",function(){
if(_11){
clearTimeout(_11);
}
_11=setTimeout(function(){
_e();
_11=null;
},0);
});
},hide:function(){
if(this._moveHandle){
this.disconnect(this._moveHandle);
this._moveHandle=null;
}
if(_3("webkit")){
var _12=this.connect(this.domNode,"webkitTransitionEnd",function(){
this.disconnect(_12);
_5.replace(this.domNode,["mblOverlayHidden"],["mblRevealv","mblOut","mblReverse","mblTransition"]);
});
_5.replace(this.domNode,["mblRevealv","mblOut","mblReverse"],["mblCoverv","mblIn","mblTransition"]);
var _13=this.domNode;
setTimeout(function(){
_5.add(_13,"mblTransition");
},100);
}else{
_5.replace(this.domNode,["mblOverlayHidden"],["mblCoverv","mblIn","mblRevealv","mblOut","mblReverse"]);
}
},onBlur:function(e){
return false;
}});
});
