//>>built
define("dojox/gfx/canvasext",["./_base","./canvas"],function(_1,_2){
var _3=_1.canvasext={};
_2.Surface.extend({getImageData:function(_4){
if("pendingRender" in this){
this._render(true);
}
return this.rawNode.getContext("2d").getImageData(_4.x,_4.y,_4.width,_4.height);
},getContext:function(){
return this.rawNode.getContext("2d");
}});
return _3;
});
