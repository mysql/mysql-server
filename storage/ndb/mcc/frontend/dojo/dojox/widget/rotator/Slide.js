//>>built
define("dojox/widget/rotator/Slide",["dojo/_base/lang","dojo/_base/fx","dojo/dom-style"],function(_1,_2,_3){
var _4=0,_5=1,UP=2,_6=3;
function _7(_8,_9){
var _a=_9.node=_9.next.node,r=_9.rotatorBox,m=_8%2,s=(m?r.w:r.h)*(_8<2?-1:1);
_3.set(_a,{display:"",zIndex:(_3.get(_9.current.node,"zIndex")||1)+1});
if(!_9.properties){
_9.properties={};
}
_9.properties[m?"left":"top"]={start:s,end:0};
return _2.animateProperty(_9);
};
var _b={slideDown:function(_c){
return _7(_4,_c);
},slideRight:function(_d){
return _7(_5,_d);
},slideUp:function(_e){
return _7(UP,_e);
},slideLeft:function(_f){
return _7(_6,_f);
}};
_1.mixin(_1.getObject("dojox.widget.rotator"),_b);
return _b;
});
