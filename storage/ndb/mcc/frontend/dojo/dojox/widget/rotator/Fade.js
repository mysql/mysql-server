//>>built
define("dojox/widget/rotator/Fade",["dojo/_base/lang","dojo/_base/fx","dojo/dom-style","dojo/fx"],function(_1,_2,_3,fx){
function _4(_5,_6){
var n=_5.next.node;
_3.set(n,{display:"",opacity:0});
_5.node=_5.current.node;
return fx[_6]([_2.fadeOut(_5),_2.fadeIn(_1.mixin(_5,{node:n}))]);
};
var _7={fade:function(_8){
return _4(_8,"chain");
},crossFade:function(_9){
return _4(_9,"combine");
}};
_1.mixin(_1.getObject("dojox.widget.rotator"),_7);
return _7;
});
