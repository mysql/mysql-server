//>>built
define("dojox/mobile/bidi/Icon",["dojo/_base/declare","dojo/dom-style","../_css3"],function(_1,_2,_3){
return _1(null,{_setCustomTransform:function(){
if((this.dir||_2.get(this.domNode,"direction"))=="rtl"){
_2.set(this.domNode.firstChild,_3.add({"direction":"ltr"},{}));
_2.set(this.domNode,_3.add({},{transform:"scaleX(-1)"}));
}
}});
});
