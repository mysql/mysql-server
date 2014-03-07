//>>built
define("dojox/fx/ext-dojo/NodeList-style",["dojo/_base/lang","dojo/_base/NodeList","dojo/NodeList-fx","dojo/fx","../style"],function(_1,_2,_3,_4,_5){
_1.extend(_2,{addClassFx:function(_6,_7){
return _4.combine(this.map(function(n){
return _5.addClass(n,_6,_7);
}));
},removeClassFx:function(_8,_9){
return _4.combine(this.map(function(n){
return _5.removeClass(n,_8,_9);
}));
},toggleClassFx:function(_a,_b,_c){
return _4.combine(this.map(function(n){
return _5.toggleClass(n,_a,_b,_c);
}));
}});
return _2;
});
