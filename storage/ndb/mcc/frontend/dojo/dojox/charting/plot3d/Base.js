//>>built
define("dojox/charting/plot3d/Base",["dojo/_base/declare","dojo/has"],function(_1,_2){
var _3=_1("dojox.charting.plot3d.Base",null,{constructor:function(_4,_5,_6){
this.width=_4;
this.height=_5;
},setData:function(_7){
this.data=_7?_7:[];
return this;
},getDepth:function(){
return this.depth;
},generate:function(_8,_9){
}});
if(_2("dojo-bidi")){
_3.extend({_checkOrientation:function(_a){
if(_a.isMirrored){
_a.applyMirroring(_a.view,{width:this.width,height:this.height},{l:0,r:0,t:0,b:0});
}
}});
}
return _3;
});
