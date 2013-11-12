//>>built
define("dojox/mobile/Opener",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-geometry","./Tooltip","./Overlay"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_4.contains(_3.doc.documentElement,"dj_phone");
var _b=_1("dojox.mobile.Opener",_a?_9:_8,{buildRendering:function(){
this.inherited(arguments);
this.cover=_5.create("div",{onclick:_2.hitch(this,"_onBlur"),"class":"mblOpenerUnderlay",style:{top:"0px",left:"0px",width:"0px",height:"0px",position:_a?"absolute":"fixed",backgroundColor:"transparent",overflow:"hidden",zIndex:"-1"}},this.domNode,"first");
this.connect(null,_3.global.onorientationchange!==undefined?"onorientationchange":"onresize",_2.hitch(this,function(){
if(_6.get(this.cover,"height")!=="0px"){
this._resizeCover();
}
}));
},onShow:function(_c){
},onHide:function(_d,v){
},show:function(_e,_f){
this.node=_e;
this.onShow(_e);
this._resizeCover();
return this.inherited(arguments);
},hide:function(val){
this.inherited(arguments);
_6.set(this.cover,{height:"0px"});
this.onHide(this.node,val);
},_resizeCover:function(){
if(_a){
_6.set(this.cover,{height:"0px"});
setTimeout(_2.hitch(this,function(){
var pos=_7.position(this.domNode,false);
_6.set(this.cover,{top:-pos.y+"px",left:-pos.x+"px",width:(pos.w+pos.x)+"px",height:(pos.h+pos.y)+"px"});
}),0);
}else{
_6.set(this.cover,{width:Math.max(_3.doc.documentElement.scrollWidth||_3.body().scrollWidth||_3.doc.documentElement.clientWidth)+"px",height:Math.max(_3.doc.documentElement.scrollHeight||_3.body().scrollHeight||_3.doc.documentElement.clientHeight)+"px"});
}
},_onBlur:function(e){
var ret=this.onBlur(e);
if(ret!==false){
this.hide(e);
}
return ret;
}});
_b.prototype.baseClass+=" mblOpener";
return _b;
});
