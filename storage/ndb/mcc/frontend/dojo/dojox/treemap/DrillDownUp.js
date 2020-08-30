//>>built
define("dojox/treemap/DrillDownUp",["dojo/_base/lang","dojo/_base/event","dojo/_base/declare","dojo/on","dojo/dom-geometry","dojo/dom-construct","dojo/dom-style","dojo/_base/fx","dojo/has!touch?dojox/gesture/tap"],function(_1,_2,_3,on,_4,_5,_6,fx,_7){
return _3("dojox.treemap.DrillDownUp",null,{postCreate:function(){
this.inherited(arguments);
this.own(on(this.domNode,"dblclick",_1.hitch(this,this._onDoubleClick)));
if(_7){
this.own(on(this.domNode,_7.doubletap,_1.hitch(this,this._onDoubleClick)));
}
},_onDoubleClick:function(e){
var _8=this._getRendererFromTarget(e.target);
if(_8.item){
var _9=_8.item;
if(this._isLeaf(_9)){
_9=_8.parentItem;
_8=this.itemToRenderer[this.getIdentity(_9)];
if(_8==null){
return;
}
}
if(this.rootItem==_9){
this.drillUp(_8);
}else{
this.drillDown(_8);
}
_2.stop(e);
}
},drillUp:function(_a){
var _b=_a.item;
this.domNode.removeChild(_a);
var _c=this._getRenderer(_b).parentItem;
this.set("rootItem",_c);
this.validateRendering();
_5.place(_a,this.domNode);
_6.set(_a,"zIndex",40);
var _d=_4.position(this._getRenderer(_b),true);
var _e=_4.getMarginBox(this.domNode);
fx.animateProperty({node:_a,duration:500,properties:{left:{end:_d.x-_e.l},top:{end:_d.y-_e.t},height:{end:_d.h},width:{end:_d.w}},onAnimate:_1.hitch(this,function(_f){
var box=_4.getContentBox(_a);
this._layoutGroupContent(_a,box.w,box.h,_a.level+1,false,true);
}),onEnd:_1.hitch(this,function(){
this.domNode.removeChild(_a);
})}).play();
},drillDown:function(_10){
var box=_4.getMarginBox(this.domNode);
var _11=_10.item;
var _12=_10.parentNode;
var _13=_4.position(_10,true);
_12.removeChild(_10);
_5.place(_10,this.domNode);
_6.set(_10,{left:(_13.x-box.l)+"px",top:(_13.y-box.t)+"px"});
var _14=_6.get(_10,"zIndex");
_6.set(_10,"zIndex",40);
fx.animateProperty({node:_10,duration:500,properties:{left:{end:box.l},top:{end:box.t},height:{end:box.h},width:{end:box.w}},onAnimate:_1.hitch(this,function(_15){
var _16=_4.getContentBox(_10);
this._layoutGroupContent(_10,_16.w,_16.h,_10.level+1,false);
}),onEnd:_1.hitch(this,function(){
_6.set(_10,"zIndex",_14);
this.set("rootItem",_11);
})}).play();
}});
});
