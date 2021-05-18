//>>built
define("dojox/mobile/bidi/ToolBarButton",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class"],function(_1,_2,_3){
return _1(null,{buildRendering:function(){
this.inherited(arguments);
if(!this.isLeftToRight()&&this.arrow){
var _4=(this.arrow==="left"?"mblToolBarButtonLeftArrow":"mblToolBarButtonRightArrow");
var _5=(this.arrow==="left"?"mblToolBarButtonHasLeftArrow":"mblToolBarButtonHasRightArrow");
var _6=(this.arrow==="left"?"mblToolBarButtonRightArrow":"mblToolBarButtonLeftArrow");
var _7=(this.arrow==="left"?"mblToolBarButtonHasRightArrow":"mblToolBarButtonHasLeftArrow");
_3.remove(this.arrowNode,_4);
_3.add(this.arrowNode,_6);
_3.remove(this.domNode,_5);
_3.add(this.domNode,_7);
}
},_setLabelAttr:function(_8){
this.inherited(arguments);
if(!this.isLeftToRight()){
_3.toggle(this.tableNode,"mblToolBarButtonTextRtl",_8||this.arrow);
}
}});
});
