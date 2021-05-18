//>>built
define("dojox/mobile/bidi/FormLayout",["dojo/_base/declare","dojo/dom-class"],function(_1,_2){
return _1(null,{buildRendering:function(){
this.inherited(arguments);
if(!this.isLeftToRight()&&this.rightAlign){
_2.add(this.domNode,"mblFormLayoutRightAlignRtl");
}
}});
});
