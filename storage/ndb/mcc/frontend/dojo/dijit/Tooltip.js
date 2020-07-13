//>>built
require({cache:{"url:dijit/templates/Tooltip.html":"<div class=\"dijitTooltip dijitTooltipLeft\" id=\"dojoTooltip\" data-dojo-attach-event=\"mouseenter:onMouseEnter,mouseleave:onMouseLeave\"\n\t><div class=\"dijitTooltipConnector\" data-dojo-attach-point=\"connectorNode\"></div\n\t><div class=\"dijitTooltipContainer dijitTooltipContents\" data-dojo-attach-point=\"containerNode\" role='alert'></div\n></div>\n"}});
define("dijit/Tooltip",["dojo/_base/array","dojo/_base/declare","dojo/_base/fx","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/lang","dojo/mouse","dojo/on","dojo/sniff","./_base/manager","./place","./_Widget","./_TemplatedMixin","./BackgroundIframe","dojo/text!./templates/Tooltip.html","./main"],function(_1,_2,fx,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2("dijit._MasterTooltip",[_c,_d],{duration:_a.defaultDuration,templateString:_f,postCreate:function(){
this.ownerDocumentBody.appendChild(this.domNode);
this.bgIframe=new _e(this.domNode);
this.fadeIn=fx.fadeIn({node:this.domNode,duration:this.duration,onEnd:_7.hitch(this,"_onShow")});
this.fadeOut=fx.fadeOut({node:this.domNode,duration:this.duration,onEnd:_7.hitch(this,"_onHide")});
},show:function(_12,_13,_14,rtl,_15,_16,_17){
if(this.aroundNode&&this.aroundNode===_13&&this.containerNode.innerHTML==_12){
return;
}
if(this.fadeOut.status()=="playing"){
this._onDeck=arguments;
return;
}
this.containerNode.innerHTML=_12;
if(_15){
this.set("textDir",_15);
}
this.containerNode.align=rtl?"right":"left";
var pos=_b.around(this.domNode,_13,_14&&_14.length?_14:_18.defaultPosition,!rtl,_7.hitch(this,"orient"));
var _19=pos.aroundNodePos;
if(pos.corner.charAt(0)=="M"&&pos.aroundCorner.charAt(0)=="M"){
this.connectorNode.style.top=_19.y+((_19.h-this.connectorNode.offsetHeight)>>1)-pos.y+"px";
this.connectorNode.style.left="";
}else{
if(pos.corner.charAt(1)=="M"&&pos.aroundCorner.charAt(1)=="M"){
this.connectorNode.style.left=_19.x+((_19.w-this.connectorNode.offsetWidth)>>1)-pos.x+"px";
}else{
this.connectorNode.style.left="";
this.connectorNode.style.top="";
}
}
_6.set(this.domNode,"opacity",0);
this.fadeIn.play();
this.isShowingNow=true;
this.aroundNode=_13;
this.onMouseEnter=_16||_1a;
this.onMouseLeave=_17||_1a;
},orient:function(_1b,_1c,_1d,_1e,_1f){
this.connectorNode.style.top="";
var _20=_1e.h,_21=_1e.w;
_1b.className="dijitTooltip "+{"MR-ML":"dijitTooltipRight","ML-MR":"dijitTooltipLeft","TM-BM":"dijitTooltipAbove","BM-TM":"dijitTooltipBelow","BL-TL":"dijitTooltipBelow dijitTooltipABLeft","TL-BL":"dijitTooltipAbove dijitTooltipABLeft","BR-TR":"dijitTooltipBelow dijitTooltipABRight","TR-BR":"dijitTooltipAbove dijitTooltipABRight","BR-BL":"dijitTooltipRight","BL-BR":"dijitTooltipLeft"}[_1c+"-"+_1d];
this.domNode.style.width="auto";
var _22=_5.position(this.domNode);
if(_9("ie")||_9("trident")){
_22.w+=2;
}
var _23=Math.min((Math.max(_21,1)),_22.w);
_5.setMarginBox(this.domNode,{w:_23});
if(_1d.charAt(0)=="B"&&_1c.charAt(0)=="B"){
var bb=_5.position(_1b);
var _24=this.connectorNode.offsetHeight;
if(bb.h>_20){
var _25=_20-((_1f.h+_24)>>1);
this.connectorNode.style.top=_25+"px";
this.connectorNode.style.bottom="";
}else{
this.connectorNode.style.bottom=Math.min(Math.max(_1f.h/2-_24/2,0),bb.h-_24)+"px";
this.connectorNode.style.top="";
}
}else{
this.connectorNode.style.top="";
this.connectorNode.style.bottom="";
}
return Math.max(0,_22.w-_21);
},_onShow:function(){
if(_9("ie")){
this.domNode.style.filter="";
}
},hide:function(_26){
if(this._onDeck&&this._onDeck[1]==_26){
this._onDeck=null;
}else{
if(this.aroundNode===_26){
this.fadeIn.stop();
this.isShowingNow=false;
this.aroundNode=null;
this.fadeOut.play();
}else{
}
}
this.onMouseEnter=this.onMouseLeave=_1a;
},_onHide:function(){
this.domNode.style.cssText="";
this.containerNode.innerHTML="";
if(this._onDeck){
this.show.apply(this,this._onDeck);
this._onDeck=null;
}
}});
if(_9("dojo-bidi")){
_11.extend({_setAutoTextDir:function(_27){
this.applyTextDir(_27);
_1.forEach(_27.children,function(_28){
this._setAutoTextDir(_28);
},this);
},_setTextDirAttr:function(_29){
this._set("textDir",_29);
if(_29=="auto"){
this._setAutoTextDir(this.containerNode);
}else{
this.containerNode.dir=this.textDir;
}
}});
}
_10.showTooltip=function(_2a,_2b,_2c,rtl,_2d,_2e,_2f){
if(_2c){
_2c=_1.map(_2c,function(val){
return {after:"after-centered",before:"before-centered"}[val]||val;
});
}
if(!_18._masterTT){
_10._masterTT=_18._masterTT=new _11();
}
return _18._masterTT.show(_2a,_2b,_2c,rtl,_2d,_2e,_2f);
};
_10.hideTooltip=function(_30){
return _18._masterTT&&_18._masterTT.hide(_30);
};
var _31="DORMANT",_32="SHOW TIMER",_33="SHOWING",_34="HIDE TIMER";
function _1a(){
};
var _18=_2("dijit.Tooltip",_c,{label:"",showDelay:400,hideDelay:400,connectId:[],position:[],selector:"",_setConnectIdAttr:function(_35){
_1.forEach(this._connections||[],function(_36){
_1.forEach(_36,function(_37){
_37.remove();
});
},this);
this._connectIds=_1.filter(_7.isArrayLike(_35)?_35:(_35?[_35]:[]),function(id){
return _3.byId(id,this.ownerDocument);
},this);
this._connections=_1.map(this._connectIds,function(id){
var _38=_3.byId(id,this.ownerDocument),_39=this.selector,_3a=_39?function(_3b){
return on.selector(_39,_3b);
}:function(_3c){
return _3c;
},_3d=this;
return [on(_38,_3a(_8.enter),function(){
_3d._onHover(this);
}),on(_38,_3a("focusin"),function(){
_3d._onHover(this);
}),on(_38,_3a(_8.leave),_7.hitch(_3d,"_onUnHover")),on(_38,_3a("focusout"),_7.hitch(_3d,"set","state",_31))];
},this);
this._set("connectId",_35);
},addTarget:function(_3e){
var id=_3e.id||_3e;
if(_1.indexOf(this._connectIds,id)==-1){
this.set("connectId",this._connectIds.concat(id));
}
},removeTarget:function(_3f){
var id=_3f.id||_3f,idx=_1.indexOf(this._connectIds,id);
if(idx>=0){
this._connectIds.splice(idx,1);
this.set("connectId",this._connectIds);
}
},buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitTooltipData");
},startup:function(){
this.inherited(arguments);
var ids=this.connectId;
_1.forEach(_7.isArrayLike(ids)?ids:[ids],this.addTarget,this);
},getContent:function(_40){
return this.label||this.domNode.innerHTML;
},state:_31,_setStateAttr:function(val){
if(this.state==val||(val==_32&&this.state==_33)||(val==_34&&this.state==_31)){
return;
}
if(this._hideTimer){
this._hideTimer.remove();
delete this._hideTimer;
}
if(this._showTimer){
this._showTimer.remove();
delete this._showTimer;
}
switch(val){
case _31:
if(this._connectNode){
_18.hide(this._connectNode);
delete this._connectNode;
this.onHide();
}
break;
case _32:
if(this.state!=_33){
this._showTimer=this.defer(function(){
this.set("state",_33);
},this.showDelay);
}
break;
case _33:
var _41=this.getContent(this._connectNode);
if(!_41){
this.set("state",_31);
return;
}
_18.show(_41,this._connectNode,this.position,!this.isLeftToRight(),this.textDir,_7.hitch(this,"set","state",_33),_7.hitch(this,"set","state",_34));
this.onShow(this._connectNode,this.position);
break;
case _34:
this._hideTimer=this.defer(function(){
this.set("state",_31);
},this.hideDelay);
break;
}
this._set("state",val);
},_onHover:function(_42){
if(this._connectNode&&_42!=this._connectNode){
this.set("state",_31);
}
this._connectNode=_42;
this.set("state",_32);
},_onUnHover:function(_43){
this.set("state",_34);
},open:function(_44){
this.set("state",_31);
this._connectNode=_44;
this.set("state",_33);
},close:function(){
this.set("state",_31);
},onShow:function(){
},onHide:function(){
},destroy:function(){
this.set("state",_31);
_1.forEach(this._connections||[],function(_45){
_1.forEach(_45,function(_46){
_46.remove();
});
},this);
this.inherited(arguments);
}});
_18._MasterTooltip=_11;
_18.show=_10.showTooltip;
_18.hide=_10.hideTooltip;
_18.defaultPosition=["after-centered","before-centered"];
return _18;
});
