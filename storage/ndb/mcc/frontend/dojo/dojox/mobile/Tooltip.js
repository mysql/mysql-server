//>>built
define("dojox/mobile/Tooltip",["dojo/_base/array","dijit/registry","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/place","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _3("dojox.mobile.Tooltip",_a,{baseClass:"mblTooltip mblTooltipHidden",buildRendering:function(){
this.inherited(arguments);
this.anchor=_6.create("div",{"class":"mblTooltipAnchor"},this.domNode,"first");
this.arrow=_6.create("div",{"class":"mblTooltipArrow"},this.anchor);
this.innerArrow=_6.create("div",{"class":"mblTooltipInnerArrow"},this.anchor);
},show:function(_b,_c){
var _d=this.domNode;
var _e={"MRM":"mblTooltipAfter","MLM":"mblTooltipBefore","BMT":"mblTooltipBelow","TMB":"mblTooltipAbove","BLT":"mblTooltipBelow","TLB":"mblTooltipAbove","BRT":"mblTooltipBelow","TRB":"mblTooltipAbove","TLT":"mblTooltipBefore","TRT":"mblTooltipAfter","BRB":"mblTooltipAfter","BLB":"mblTooltipBefore"};
_5.remove(_d,["mblTooltipAfter","mblTooltipBefore","mblTooltipBelow","mblTooltipAbove"]);
_1.forEach(_2.findWidgets(_d),function(_f){
if(_f.height=="auto"&&typeof _f.resize=="function"){
if(!_f.fixedFooterHeight){
_f.fixedFooterHeight=_7.getPadBorderExtents(_d).b;
}
_f.resize();
}
});
if(_c){
_c=_1.map(_c,function(pos){
return {after:"after-centered",before:"before-centered"}[pos]||pos;
});
}
var _10=_9.around(_d,_b,_c||["below-centered","above-centered","after-centered","before-centered"],this.isLeftToRight());
var _11=_e[_10.corner+_10.aroundCorner.charAt(0)]||"";
_5.add(_d,_11);
var pos=_7.position(_b,true);
_8.set(this.anchor,(_11=="mblTooltipAbove"||_11=="mblTooltipBelow")?{top:"",left:Math.max(0,pos.x-_10.x+(pos.w>>1)-(this.arrow.offsetWidth>>1))+"px"}:{left:"",top:Math.max(0,pos.y-_10.y+(pos.h>>1)-(this.arrow.offsetHeight>>1))+"px"});
_5.replace(_d,"mblTooltipVisible","mblTooltipHidden");
this.resize=_4.hitch(this,"show",_b,_c);
return _10;
},hide:function(){
this.resize=undefined;
_5.replace(this.domNode,"mblTooltipHidden","mblTooltipVisible");
},onBlur:function(e){
return true;
},destroy:function(){
if(this.anchor){
this.anchor.removeChild(this.innerArrow);
this.anchor.removeChild(this.arrow);
this.domNode.removeChild(this.anchor);
this.anchor=this.arrow=this.innerArrow=undefined;
}
this.inherited(arguments);
}});
});
