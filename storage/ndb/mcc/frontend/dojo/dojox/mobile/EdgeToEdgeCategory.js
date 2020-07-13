//>>built
define("dojox/mobile/EdgeToEdgeCategory",["dojo/_base/declare","./RoundRectCategory"],function(_1,_2){
return _1("dojox.mobile.EdgeToEdgeCategory",_2,{buildRendering:function(){
this.inherited(arguments);
this.domNode.className="mblEdgeToEdgeCategory";
if(this.type&&this.type=="long"){
this.domNode.className+=" mblEdgeToEdgeCategoryLong";
}
}});
});
