//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/fx"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.Fade");
_2.require("dojo.fx");
(function(d){
function _4(_5,_6){
var n=_5.next.node;
d.style(n,{display:"",opacity:0});
_5.node=_5.current.node;
return d.fx[_6]([d.fadeOut(_5),d.fadeIn(d.mixin(_5,{node:n}))]);
};
d.mixin(_3.widget.rotator,{fade:function(_7){
return _4(_7,"chain");
},crossFade:function(_8){
return _4(_8,"combine");
}});
})(_2);
});
