//>>built
define("dojox/fx/ext-dojo/NodeList-style",["dojo/_base/lang","dojo/query","dojo/NodeList-fx","dojo/fx","../style"],function(_1,_2,_3,_4,_5){
var _6=_2.NodeList;
_1.extend(_6,{addClassFx:function(_7,_8){
return _4.combine(this.map(function(n){
return _5.addClass(n,_7,_8);
}));
},removeClassFx:function(_9,_a){
return _4.combine(this.map(function(n){
return _5.removeClass(n,_9,_a);
}));
},toggleClassFx:function(_b,_c,_d){
return _4.combine(this.map(function(n){
return _5.toggleClass(n,_b,_c,_d);
}));
}});
return _6;
});
