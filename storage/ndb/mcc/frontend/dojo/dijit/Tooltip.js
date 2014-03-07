//>>built
require({cache:{"url:dijit/templates/Tooltip.html":"<div class=\"dijitTooltip dijitTooltipLeft\" id=\"dojoTooltip\"\n\t><div class=\"dijitTooltipContainer dijitTooltipContents\" data-dojo-attach-point=\"containerNode\" role='alert'></div\n\t><div class=\"dijitTooltipConnector\" data-dojo-attach-point=\"connectorNode\"></div\n></div>\n"}});
define("dijit/Tooltip",["dojo/_base/array","dojo/_base/declare","dojo/_base/fx","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","./_base/manager","./place","./_Widget","./_TemplatedMixin","./BackgroundIframe","dojo/text!./templates/Tooltip.html","."],function(_1,_2,fx,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2("dijit._MasterTooltip",[_c,_d],{duration:_a.defaultDuration,templateString:_f,postCreate:function(){
_9.body().appendChild(this.domNode);
this.bgIframe=new _e(this.domNode);
this.fadeIn=fx.fadeIn({node:this.domNode,duration:this.duration,onEnd:_7.hitch(this,"_onShow")});
this.fadeOut=fx.fadeOut({node:this.domNode,duration:this.duration,onEnd:_7.hitch(this,"_onHide")});
},show:function(_12,_13,_14,rtl,_15){
if(this.aroundNode&&this.aroundNode===_13&&this.containerNode.innerHTML==_12){
return;
}
this.domNode.width="auto";
if(this.fadeOut.status()=="playing"){
this._onDeck=arguments;
return;
}
this.containerNode.innerHTML=_12;
this.set("textDir",_15);
this.containerNode.align=rtl?"right":"left";
var pos=_b.around(this.domNode,_13,_14&&_14.length?_14:_16.defaultPosition,!rtl,_7.hitch(this,"orient"));
var _17=pos.aroundNodePos;
if(pos.corner.charAt(0)=="M"&&pos.aroundCorner.charAt(0)=="M"){
this.connectorNode.style.top=_17.y+((_17.h-this.connectorNode.offsetHeight)>>1)-pos.y+"px";
this.connectorNode.style.left="";
}else{
if(pos.corner.charAt(1)=="M"&&pos.aroundCorner.charAt(1)=="M"){
this.connectorNode.style.left=_17.x+((_17.w-this.connectorNode.offsetWidth)>>1)-pos.x+"px";
}
}
_6.set(this.domNode,"opacity",0);
this.fadeIn.play();
this.isShowingNow=true;
this.aroundNode=_13;
},orient:function(_18,_19,_1a,_1b,_1c){
this.connectorNode.style.top="";
var _1d=_1b.w-this.connectorNode.offsetWidth;
_18.className="dijitTooltip "+{"MR-ML":"dijitTooltipRight","ML-MR":"dijitTooltipLeft","TM-BM":"dijitTooltipAbove","BM-TM":"dijitTooltipBelow","BL-TL":"dijitTooltipBelow dijitTooltipABLeft","TL-BL":"dijitTooltipAbove dijitTooltipABLeft","BR-TR":"dijitTooltipBelow dijitTooltipABRight","TR-BR":"dijitTooltipAbove dijitTooltipABRight","BR-BL":"dijitTooltipRight","BL-BR":"dijitTooltipLeft"}[_19+"-"+_1a];
this.domNode.style.width="auto";
var _1e=_5.getContentBox(this.domNode);
var _1f=Math.min((Math.max(_1d,1)),_1e.w);
var _20=_1f<_1e.w;
this.domNode.style.width=_1f+"px";
if(_20){
this.containerNode.style.overflow="auto";
var _21=this.containerNode.scrollWidth;
this.containerNode.style.overflow="visible";
if(_21>_1f){
_21=_21+_6.get(this.domNode,"paddingLeft")+_6.get(this.domNode,"paddingRight");
this.domNode.style.width=_21+"px";
}
}
if(_1a.charAt(0)=="B"&&_19.charAt(0)=="B"){
var mb=_5.getMarginBox(_18);
var _22=this.connectorNode.offsetHeight;
if(mb.h>_1b.h){
var _23=_1b.h-((_1c.h+_22)>>1);
this.connectorNode.style.top=_23+"px";
this.connectorNode.style.bottom="";
}else{
this.connectorNode.style.bottom=Math.min(Math.max(_1c.h/2-_22/2,0),mb.h-_22)+"px";
this.connectorNode.style.top="";
}
}else{
this.connectorNode.style.top="";
this.connectorNode.style.bottom="";
}
return Math.max(0,_1e.w-_1d);
},_onShow:function(){
if(_8("ie")){
this.domNode.style.filter="";
}
},hide:function(_24){
if(this._onDeck&&this._onDeck[1]==_24){
this._onDeck=null;
}else{
if(this.aroundNode===_24){
this.fadeIn.stop();
this.isShowingNow=false;
this.aroundNode=null;
this.fadeOut.play();
}else{
}
}
},_onHide:function(){
this.domNode.style.cssText="";
this.containerNode.innerHTML="";
if(this._onDeck){
this.show.apply(this,this._onDeck);
this._onDeck=null;
}
},_setAutoTextDir:function(_25){
this.applyTextDir(_25,_8("ie")?_25.outerText:_25.textContent);
_1.forEach(_25.children,function(_26){
this._setAutoTextDir(_26);
},this);
},_setTextDirAttr:function(_27){
this._set("textDir",typeof _27!="undefined"?_27:"");
if(_27=="auto"){
this._setAutoTextDir(this.containerNode);
}else{
this.containerNode.dir=this.textDir;
}
}});
_10.showTooltip=function(_28,_29,_2a,rtl,_2b){
if(!_16._masterTT){
_10._masterTT=_16._masterTT=new _11();
}
return _16._masterTT.show(_28,_29,_2a,rtl,_2b);
};
_10.hideTooltip=function(_2c){
return _16._masterTT&&_16._masterTT.hide(_2c);
};
var _16=_2("dijit.Tooltip",_c,{label:"",showDelay:400,connectId:[],position:[],_setConnectIdAttr:function(_2d){
_1.forEach(this._connections||[],function(_2e){
_1.forEach(_2e,_7.hitch(this,"disconnect"));
},this);
this._connectIds=_1.filter(_7.isArrayLike(_2d)?_2d:(_2d?[_2d]:[]),function(id){
return _3.byId(id);
});
this._connections=_1.map(this._connectIds,function(id){
var _2f=_3.byId(id);
return [this.connect(_2f,"onmouseenter","_onHover"),this.connect(_2f,"onmouseleave","_onUnHover"),this.connect(_2f,"onfocus","_onHover"),this.connect(_2f,"onblur","_onUnHover")];
},this);
this._set("connectId",_2d);
},addTarget:function(_30){
var id=_30.id||_30;
if(_1.indexOf(this._connectIds,id)==-1){
this.set("connectId",this._connectIds.concat(id));
}
},removeTarget:function(_31){
var id=_31.id||_31,idx=_1.indexOf(this._connectIds,id);
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
},_onHover:function(e){
if(!this._showTimer){
var _32=e.target;
this._showTimer=setTimeout(_7.hitch(this,function(){
this.open(_32);
}),this.showDelay);
}
},_onUnHover:function(){
if(this._focus){
return;
}
if(this._showTimer){
clearTimeout(this._showTimer);
delete this._showTimer;
}
this.close();
},open:function(_33){
if(this._showTimer){
clearTimeout(this._showTimer);
delete this._showTimer;
}
_16.show(this.label||this.domNode.innerHTML,_33,this.position,!this.isLeftToRight(),this.textDir);
this._connectNode=_33;
this.onShow(_33,this.position);
},close:function(){
if(this._connectNode){
_16.hide(this._connectNode);
delete this._connectNode;
this.onHide();
}
if(this._showTimer){
clearTimeout(this._showTimer);
delete this._showTimer;
}
},onShow:function(){
},onHide:function(){
},uninitialize:function(){
this.close();
this.inherited(arguments);
}});
_16._MasterTooltip=_11;
_16.show=_10.showTooltip;
_16.hide=_10.hideTooltip;
_16.defaultPosition=["after","before"];
return _16;
});
