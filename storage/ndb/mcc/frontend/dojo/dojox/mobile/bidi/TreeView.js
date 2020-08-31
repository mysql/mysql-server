//>>built
define("dojox/mobile/bidi/TreeView",["dojo/_base/declare"],function(_1){
return _1(null,{_customizeListItem:function(_2){
_2.textDir=this.textDir;
if(!this.isLeftToRight()){
_2.dir="rtl";
_2.transitionDir=-1;
}
}});
});
