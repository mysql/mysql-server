//>>built
require({cache:{"url:dojox/layout/resources/ExpandoPane.html":"<div class=\"dojoxExpandoPane\">\n\t<div dojoAttachPoint=\"titleWrapper\" class=\"dojoxExpandoTitle\">\n\t\t<div class=\"dojoxExpandoIcon\" dojoAttachPoint=\"iconNode\" dojoAttachEvent=\"onclick:toggle\"><span class=\"a11yNode\">X</span></div>\t\t\t\n\t\t<span class=\"dojoxExpandoTitleNode\" dojoAttachPoint=\"titleNode\">${title}</span>\n\t</div>\n\t<div class=\"dojoxExpandoWrapper\" dojoAttachPoint=\"cwrapper\" dojoAttachEvent=\"ondblclick:_trap\">\n\t\t<div class=\"dojoxExpandoContent\" dojoAttachPoint=\"containerNode\"></div>\n\t</div>\n</div>\n"}});
define("dojox/layout/ExpandoPane",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/fx","dojo/dom-style","dojo/dom-class","dojo/dom-geometry","dojo/text!./resources/ExpandoPane.html","dijit/layout/ContentPane","dijit/_TemplatedMixin","dijit/_Contained","dijit/_Container"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
_1.experimental("dojox.layout.ExpandoPane");
return _3("dojox.layout.ExpandoPane",[_c,_d,_e,_f],{attributeMap:_2.delegate(_c.prototype.attributeMap,{title:{node:"titleNode",type:"innerHTML"}}),templateString:_b,easeOut:"dojo._DefaultEasing",easeIn:"dojo._DefaultEasing",duration:420,startExpanded:true,previewOpacity:0.75,previewOnDblClick:false,baseClass:"dijitExpandoPane",postCreate:function(){
this.inherited(arguments);
this._animConnects=[];
this._isHorizontal=true;
if(_2.isString(this.easeOut)){
this.easeOut=_2.getObject(this.easeOut);
}
if(_2.isString(this.easeIn)){
this.easeIn=_2.getObject(this.easeIn);
}
var _10="",rtl=!this.isLeftToRight();
if(this.region){
switch(this.region){
case "trailing":
case "right":
_10=rtl?"Left":"Right";
break;
case "leading":
case "left":
_10=rtl?"Right":"Left";
break;
case "top":
_10="Top";
break;
case "bottom":
_10="Bottom";
break;
}
_9.add(this.domNode,"dojoxExpando"+_10);
_9.add(this.iconNode,"dojoxExpandoIcon"+_10);
this._isHorizontal=/top|bottom/.test(this.region);
}
_8.set(this.domNode,{overflow:"hidden",padding:0});
this.connect(this.domNode,"ondblclick",this.previewOnDblClick?"preview":"toggle");
if(this.previewOnDblClick){
this.connect(this.getParent(),"_layoutChildren",_2.hitch(this,function(){
this._isonlypreview=false;
}));
}
},_startupSizes:function(){
this._container=this.getParent();
this._closedSize=this._titleHeight=_a.getMarginBox(this.titleWrapper).h;
if(this.splitter){
var _11=this.id;
_4.forEach(dijit.registry.toArray(),function(w){
if(w&&w.child&&w.child.id==_11){
this.connect(w,"_stopDrag","_afterResize");
}
},this);
}
this._currentSize=_a.getContentBox(this.domNode);
this._showSize=this._currentSize[(this._isHorizontal?"h":"w")];
this._setupAnims();
if(this.startExpanded){
this._showing=true;
}else{
this._showing=false;
this._hideWrapper();
this._hideAnim.gotoPercent(99,true);
}
this._hasSizes=true;
},_afterResize:function(e){
var tmp=this._currentSize;
this._currentSize=_a.getMarginBox(this.domNode);
var n=this._currentSize[(this._isHorizontal?"h":"w")];
if(n>this._titleHeight){
if(!this._showing){
this._showing=!this._showing;
this._showEnd();
}
this._showSize=n;
this._setupAnims();
}else{
this._showSize=tmp[(this._isHorizontal?"h":"w")];
this._showing=false;
this._hideWrapper();
this._hideAnim.gotoPercent(89,true);
}
},_setupAnims:function(){
_4.forEach(this._animConnects,_5.disconnect);
var _12={node:this.domNode,duration:this.duration},_13=this._isHorizontal,_14={},_15={},_16=_13?"height":"width";
_14[_16]={end:this._showSize};
_15[_16]={end:this._closedSize};
this._showAnim=_7.animateProperty(_2.mixin(_12,{easing:this.easeIn,properties:_14}));
this._hideAnim=_7.animateProperty(_2.mixin(_12,{easing:this.easeOut,properties:_15}));
this._animConnects=[_5.connect(this._showAnim,"onEnd",this,"_showEnd"),_5.connect(this._hideAnim,"onEnd",this,"_hideEnd")];
},preview:function(){
if(!this._showing){
this._isonlypreview=!this._isonlypreview;
}
this.toggle();
},toggle:function(){
if(this._showing){
this._hideWrapper();
this._showAnim&&this._showAnim.stop();
this._hideAnim.play();
}else{
this._hideAnim&&this._hideAnim.stop();
this._showAnim.play();
}
this._showing=!this._showing;
},_hideWrapper:function(){
_9.add(this.domNode,"dojoxExpandoClosed");
_8.set(this.cwrapper,{visibility:"hidden",opacity:"0",overflow:"hidden"});
},_showEnd:function(){
_8.set(this.cwrapper,{opacity:0,visibility:"visible"});
_7.anim(this.cwrapper,{opacity:this._isonlypreview?this.previewOpacity:1},227);
_9.remove(this.domNode,"dojoxExpandoClosed");
if(!this._isonlypreview){
setTimeout(_2.hitch(this._container,"layout"),15);
}else{
this._previewShowing=true;
this.resize();
}
},_hideEnd:function(){
if(!this._isonlypreview){
setTimeout(_2.hitch(this._container,"layout"),25);
}else{
this._previewShowing=false;
}
this._isonlypreview=false;
},resize:function(_17){
if(!this._hasSizes){
this._startupSizes(_17);
}
var _18=_a.getMarginBox(this.domNode);
this._contentBox={w:_17&&"w" in _17?_17.w:_18.w,h:(_17&&"h" in _17?_17.h:_18.h)-this._titleHeight};
_8.set(this.containerNode,"height",this._contentBox.h+"px");
if(_17){
_a.setMarginBox(this.domNode,_17);
}
this._layoutChildren();
},_trap:function(e){
_6.stop(e);
}});
});
