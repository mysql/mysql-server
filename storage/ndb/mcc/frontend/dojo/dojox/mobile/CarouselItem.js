//>>built
define("dojox/mobile/CarouselItem",["dojo/_base/declare","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/_Contained","dijit/_WidgetBase","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/CarouselItem"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1(_7("dojo-bidi")?"dojox.mobile.NonBidiCarouselItem":"dojox.mobile.CarouselItem",[_6,_5],{alt:"",src:"",headerText:"",footerText:"",baseClass:"mblCarouselItem",buildRendering:function(){
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
},resize:function(_a){
var _b=_3.getMarginBox(this.domNode);
if(_b.h===0){
return;
}
var h1=_3.getMarginBox(this.headerTextNode).h;
var h2=_3.getMarginBox(this.footerTextNode).h;
_3.setMarginBox(this.imageNode,{h:_b.h-h1-h2});
},select:function(){
var _c=this.imageNode;
_4.set(_c,"opacity",0.4);
this.defer(function(){
_4.set(_c,"opacity",1);
},1000);
},_setAltAttr:function(_d){
this._set("alt",_d);
this.imageNode.alt=_d;
},_setSrcAttr:function(_e){
this._set("src",_e);
this.imageNode.src=_e;
},_setHeaderTextAttr:function(_f){
this._set("headerText",_f);
this.headerTextNode.innerHTML=this._cv?this._cv(_f):_f;
},_setFooterTextAttr:function(_10){
this._set("footerText",_10);
this.footerTextNode.innerHTML=this._cv?this._cv(_10):_10;
}});
return _7("dojo-bidi")?_1("dojox.mobile.CarouselItem",[_9,_8]):_9;
});
