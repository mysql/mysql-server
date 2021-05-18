//>>built
define("dojox/mobile/Tooltip",["dojo/_base/array","dijit/registry","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/place","dijit/_WidgetBase","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Tooltip"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_3(_b("dojo-bidi")?"dojox.mobile.NonBidiTooltip":"dojox.mobile.Tooltip",_a,{baseClass:"mblTooltip mblTooltipHidden",buildRendering:function(){
this.inherited(arguments);
this.anchor=_6.create("div",{"class":"mblTooltipAnchor"},this.domNode,"first");
this.arrow=_6.create("div",{"class":"mblTooltipArrow"},this.anchor);
this.innerArrow=_6.create("div",{"class":"mblTooltipInnerArrow"},this.anchor);
if(!this.containerNode){
this.containerNode=this.domNode;
}
},show:function(_e,_f){
var _10=this.domNode;
var _11={"MRM":"mblTooltipAfter","MLM":"mblTooltipBefore","BMT":"mblTooltipBelow","TMB":"mblTooltipAbove","BLT":"mblTooltipBelow","TLB":"mblTooltipAbove","BRT":"mblTooltipBelow","TRB":"mblTooltipAbove","TLT":"mblTooltipBefore","TRT":"mblTooltipAfter","BRB":"mblTooltipAfter","BLB":"mblTooltipBefore"};
_5.remove(_10,["mblTooltipAfter","mblTooltipBefore","mblTooltipBelow","mblTooltipAbove"]);
_1.forEach(_2.findWidgets(_10),function(_12){
if(_12.height=="auto"&&typeof _12.resize=="function"){
if(!_12._parentPadBorderExtentsBottom){
_12._parentPadBorderExtentsBottom=_7.getPadBorderExtents(_10).b;
}
_12.resize();
}
});
if(_f){
_f=_1.map(_f,function(pos){
return {after:"after-centered",before:"before-centered"}[pos]||pos;
});
}
var _13=_9.around(_10,_e,_f||["below-centered","above-centered","after-centered","before-centered"],this.isLeftToRight());
var _14=_11[_13.corner+_13.aroundCorner.charAt(0)]||"";
_5.add(_10,_14);
var pos=_7.position(_e,true);
_8.set(this.anchor,(_14=="mblTooltipAbove"||_14=="mblTooltipBelow")?{top:"",left:Math.max(0,pos.x-_13.x+(pos.w>>1)-(this.arrow.offsetWidth>>1))+"px"}:{left:"",top:Math.max(0,pos.y-_13.y+(pos.h>>1)-(this.arrow.offsetHeight>>1))+"px"});
_5.replace(_10,"mblTooltipVisible","mblTooltipHidden");
this.resize=_4.hitch(this,"show",_e,_f);
return _13;
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
return _b("dojo-bidi")?_3("dojox.mobile.Tooltip",[_d,_c]):_d;
});
