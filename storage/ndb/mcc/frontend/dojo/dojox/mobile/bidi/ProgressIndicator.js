//>>built
define("dojox/mobile/bidi/ProgressIndicator",["dojo/_base/declare","dojo/dom-class"],function(_1,_2){
return _1(null,{buildRendering:function(){
this.inherited(arguments);
if(!this.isLeftToRight()){
if(this.closeButton){
var s=Math.round(this.closeButtonNode.offsetHeight/2);
this.closeButtonNode.style.left=-s+"px";
}
if(this.center){
_2.add(this.domNode,"mblProgressIndicatorCenterRtl");
}
_2.add(this.containerNode,"mblProgContainerRtl");
}
}});
});
