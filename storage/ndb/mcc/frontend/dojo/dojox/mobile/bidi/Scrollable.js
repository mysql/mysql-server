//>>built
define("dojox/mobile/bidi/Scrollable",["dojo/_base/declare"],function(_1){
return _1(null,{showScrollBar:function(){
this.inherited(arguments);
if(!this.isLeftToRight()&&this._scrollBarWrapperV){
this._scrollBarWrapperV.style.right="auto";
this._scrollBarWrapperV.style.left="2px";
}
}});
});
