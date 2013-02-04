//>>built
define("dijit/layout/_LayoutWidget",["dojo/_base/lang","../_Widget","../_Container","../_Contained","dojo/_base/declare","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/sniff","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _5("dijit.layout._LayoutWidget",[_2,_3,_4],{baseClass:"dijitLayoutContainer",isLayoutContainer:true,buildRendering:function(){
this.inherited(arguments);
_6.add(this.domNode,"dijitContainer");
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
var _b=this.getParent&&this.getParent();
if(!(_b&&_b.isLayoutContainer)){
this.resize();
this.connect(_a.global,"onresize",function(){
this.resize();
});
}
},resize:function(_c,_d){
var _e=this.domNode;
if(_c){
_7.setMarginBox(_e,_c);
}
var mb=_d||{};
_1.mixin(mb,_c||{});
if(!("h" in mb)||!("w" in mb)){
mb=_1.mixin(_7.getMarginBox(_e),mb);
}
var cs=_8.getComputedStyle(_e);
var me=_7.getMarginExtents(_e,cs);
var be=_7.getBorderExtents(_e,cs);
var bb=(this._borderBox={w:mb.w-(me.w+be.w),h:mb.h-(me.h+be.h)});
var pe=_7.getPadExtents(_e,cs);
this._contentBox={l:_8.toPixelValue(_e,cs.paddingLeft),t:_8.toPixelValue(_e,cs.paddingTop),w:bb.w-pe.w,h:bb.h-pe.h};
this.layout();
},layout:function(){
},_setupChild:function(_f){
var cls=this.baseClass+"-child "+(_f.baseClass?this.baseClass+"-"+_f.baseClass:"");
_6.add(_f.domNode,cls);
},addChild:function(_10,_11){
this.inherited(arguments);
if(this._started){
this._setupChild(_10);
}
},removeChild:function(_12){
var cls=this.baseClass+"-child"+(_12.baseClass?" "+this.baseClass+"-"+_12.baseClass:"");
_6.remove(_12.domNode,cls);
this.inherited(arguments);
}});
});
