//>>built
define("dojox/app/controllers/Layout",["dojo/_base/lang","dojo/_base/declare","dojo/sniff","dojo/on","dojo/_base/window","dojo/_base/array","dojo/_base/config","dojo/topic","dojo/query","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dijit/registry","../Controller","../layout/utils"],function(_1,_2,_3,on,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
return _2("dojox.app.controllers.Layout",_d,{constructor:function(_f,_10){
this.events={"layout":this.layout,"select":this.select};
this.inherited(arguments);
if(_6.mblHideAddressBar){
_7.subscribe("/dojox/mobile/afterResizeAll",_1.hitch(this,this.onResize));
}else{
this.bind(_4.global,_3("ios")?"orientationchange":"resize",_1.hitch(this,this.onResize));
}
},onResize:function(){
this._doResize(this.app);
},layout:function(_11){
var _12=_11.view;
var _13=_11.changeSize||null;
var _14=_11.resultSize||null;
this._doResize(_12,_13,_14);
},_doLayout:function(_15){
if(!_15){
console.warn("layout empty view.");
return;
}
var _16,_17;
if(_15.selectedChild&&_15.selectedChild.isFullScreen){
console.warn("fullscreen sceen layout");
}else{
_17=_8("> [data-app-region], > [region]",_15.domNode).map(function(_18){
var w=_c.getEnclosingWidget(_18);
if(w){
w.region=_a.get(_18,"data-app-region")||_a.get(_18,"region");
return w;
}
return {domNode:_18,region:_a.get(_18,"data-app-region")||_a.get(_18,"region")};
});
if(_15.selectedChild){
_17=_5.filter(_17,function(c){
if((c.region=="center")&&_15.selectedChild&&(_15.selectedChild.domNode!==c.domNode)){
_9.set(c.domNode,"zIndex",25);
_9.set(c.domNode,"display","none");
return false;
}else{
if(c.region!="center"){
_9.set(c.domNode,"display","");
_9.set(c.domNode,"zIndex",100);
}
}
return c.domNode&&c.region;
},_15);
}
}
if(_15._contentBox){
_e.layoutChildren(_15.domNode,_15._contentBox,_17);
}
},_doResize:function(_19,_1a,_1b){
var _1c=_19.domNode;
if(_1a){
_b.setMarginBox(_1c,_1a);
if(_1a.t){
_1c.style.top=_1a.t+"px";
}
if(_1a.l){
_1c.style.left=_1a.l+"px";
}
}
var mb=_1b||{};
_1.mixin(mb,_1a||{});
if(!("h" in mb)||!("w" in mb)){
mb=_1.mixin(_b.getMarginBox(_1c),mb);
}
if(_19!==this.app){
var cs=_9.getComputedStyle(_1c);
var me=_b.getMarginExtents(_1c,cs);
var be=_b.getBorderExtents(_1c,cs);
var bb=(_19._borderBox={w:mb.w-(me.w+be.w),h:mb.h-(me.h+be.h)});
var pe=_b.getPadExtents(_1c,cs);
_19._contentBox={l:_9.toPixelValue(_1c,cs.paddingLeft),t:_9.toPixelValue(_1c,cs.paddingTop),w:bb.w-pe.w,h:bb.h-pe.h};
}else{
_19._contentBox={l:0,t:0,h:_4.global.innerHeight||_4.doc.documentElement.clientHeight,w:_4.global.innerWidth||_4.doc.documentElement.clientWidth};
}
this._doLayout(_19);
if(_19.selectedChild){
this._doResize(_19.selectedChild);
}
},select:function(_1d){
var _1e=_1d.parent||this.app;
var _1f=_1d.view;
if(!_1f){
return;
}
if(_1f!==_1e.selectedChild){
if(_1e.selectedChild){
_9.set(_1e.selectedChild.domNode,"zIndex",25);
}
_9.set(_1f.domNode,"display","");
_9.set(_1f.domNode,"zIndex",50);
_1e.selectedChild=_1f;
}
this._doResize(_1e);
}});
});
