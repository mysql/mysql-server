//>>built
define("dojox/app/widgets/Container",["dojo/_base/declare","dojo/_base/lang","dijit/registry","dojo/dom-attr","dojo/dom-geometry","dojo/dom-style","dijit/_WidgetBase","dijit/_Container","dijit/_Contained","dojo/_base/array","dojo/query","../utils/layout","./_ScrollableMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
return _1("dojox.app.widgets.Container",[_7,_8,_9,_d],{scrollable:false,fixedFooter:"",fixedHeader:"",buildRendering:function(){
if(!this._constraint){
this._constraint="center";
_4.set(this.srcNodeRef,"data-app-constraint","center");
}
this.inherited(arguments);
_6.set(this.domNode,"overflow-x","hidden");
_6.set(this.domNode,"overflow-y","auto");
if(this.scrollable){
this.inherited(arguments);
this.domNode.style.position="absolute";
this.domNode.style.width="100%";
this.domNode.style.height="100%";
}
},startup:function(){
if(this._started){
return;
}
if(this.scrollable){
this.inherited(arguments);
}
this._started=true;
},resize:function(_e,_f){
var _10=this.domNode;
if(this.scrollable){
this.inherited(arguments);
this.layout();
return;
}
if(_e){
_5.setMarginBox(_10,_e);
}
var mb=_f||{};
_2.mixin(mb,_e||{});
if(!("h" in mb)||!("w" in mb)){
mb=_2.mixin(_5.getMarginBox(_10),mb);
}
var cs=_6.getComputedStyle(_10);
var me=_5.getMarginExtents(_10,cs);
var be=_5.getBorderExtents(_10,cs);
var bb=(this._borderBox={w:mb.w-(me.w+be.w),h:mb.h-(me.h+be.h)});
var pe=_5.getPadExtents(_10,cs);
this._contentBox={l:_6.toPixelValue(_10,cs.paddingLeft),t:_6.toPixelValue(_10,cs.paddingTop),w:bb.w-pe.w,h:bb.h-pe.h};
this.layout();
},layout:function(){
var _11=_b("> [data-app-constraint]",this.domNode).map(function(_12){
var w=_3.getEnclosingWidget(_12);
if(w){
w._constraint=_4.get(_12,"data-app-constraint");
return w;
}
return {domNode:_12,_constraint:_4.get(_12,"data-app-constraint")};
});
if(this._contentBox){
_c.layoutChildren(this.domNode,this._contentBox,_11);
}
_a.forEach(this.getChildren(),function(_13){
if(!_13._started&&_13.startup){
_13.startup();
}
});
}});
});
