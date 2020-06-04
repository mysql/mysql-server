//>>built
define("dojox/mobile/bidi/_StoreListMixin",["dojo/_base/declare"],function(_1){
return _1(null,{createListItem:function(_2){
var w=this.inherited(arguments);
w.set("textDir",this.textDir);
return w;
}});
});
