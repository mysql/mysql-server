//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.Slide");
(function(d){
var _4=0,_5=1,UP=2,_6=3;
function _7(_8,_9){
var _a=_9.node=_9.next.node,r=_9.rotatorBox,m=_8%2,s=(m?r.w:r.h)*(_8<2?-1:1);
d.style(_a,{display:"",zIndex:(d.style(_9.current.node,"zIndex")||1)+1});
if(!_9.properties){
_9.properties={};
}
_9.properties[m?"left":"top"]={start:s,end:0};
return d.animateProperty(_9);
};
d.mixin(_3.widget.rotator,{slideDown:function(_b){
return _7(_4,_b);
},slideRight:function(_c){
return _7(_5,_c);
},slideUp:function(_d){
return _7(UP,_d);
},slideLeft:function(_e){
return _7(_6,_e);
}});
})(_2);
});
