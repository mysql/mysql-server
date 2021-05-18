//>>built
define("dojox/lang/oo/general",["dojo","dijit","dojox","dojo/require!dojox/lang/oo/Decorator"],function(_1,_2,_3){
_1.provide("dojox.lang.oo.general");
_1.require("dojox.lang.oo.Decorator");
(function(){
var oo=_3.lang.oo,md=oo.makeDecorator,_4=oo.general,_5=_1.isFunction;
_4.augment=md(function(_6,_7,_8){
return typeof _8=="undefined"?_7:_8;
});
_4.override=md(function(_9,_a,_b){
return typeof _b!="undefined"?_a:_b;
});
_4.shuffle=md(function(_c,_d,_e){
return _5(_e)?function(){
return _e.apply(this,_d.apply(this,arguments));
}:_e;
});
_4.wrap=md(function(_f,_10,_11){
return function(){
return _10.call(this,_11,arguments);
};
});
_4.tap=md(function(_12,_13,_14){
return function(){
_13.apply(this,arguments);
return this;
};
});
_4.before=md(function(_15,_16,_17){
return _5(_17)?function(){
_16.apply(this,arguments);
return _17.apply(this,arguments);
}:_16;
});
_4.after=md(function(_18,_19,_1a){
return _5(_1a)?function(){
_1a.apply(this,arguments);
return _19.apply(this,arguments);
}:_19;
});
})();
});
