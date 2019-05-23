//>>built
define("dojox/mobile/CarouselItem",["dojo/_base/declare","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.mobile.CarouselItem",[_6,_5],{alt:"",src:"",headerText:"",footerText:"",baseClass:"mblCarouselItem",buildRendering:function(){
this.inherited(arguments);
this.domNode.tabIndex="0";
this.headerTextNode=_2.create("div",{className:"mblCarouselItemHeaderText"},this.domNode);
this.imageNode=_2.create("img",{className:"mblCarouselItemImage"},this.domNode);
this.footerTextNode=_2.create("div",{className:"mblCarouselItemFooterText"},this.domNode);
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.resize();
},resize:function(_8){
var _9=_3.getMarginBox(this.domNode);
if(_9.h===0){
return;
}
var h1=_3.getMarginBox(this.headerTextNode).h;
var h2=_3.getMarginBox(this.footerTextNode).h;
_3.setMarginBox(this.imageNode,{h:_9.h-h1-h2});
},select:function(){
var _a=this.imageNode;
_4.set(_a,"opacity",0.4);
setTimeout(function(){
_4.set(_a,"opacity",1);
},1000);
},_setAltAttr:function(_b){
this._set("alt",_b);
this.imageNode.alt=_b;
},_setSrcAttr:function(_c){
this._set("src",_c);
this.imageNode.src=_c;
},_setHeaderTextAttr:function(_d){
this._set("headerText",_d);
this.headerTextNode.innerHTML=this._cv?this._cv(_d):_d;
},_setFooterTextAttr:function(_e){
this._set("footerText",_e);
this.footerTextNode.innerHTML=this._cv?this._cv(_e):_e;
}});
});
