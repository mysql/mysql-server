//>>built
define("dojox/lang/oo/aop",["dojo","dijit","dojox","dojo/require!dojox/lang/oo/Decorator,dojox/lang/oo/general"],function(_1,_2,_3){
_1.provide("dojox.lang.oo.aop");
_1.require("dojox.lang.oo.Decorator");
_1.require("dojox.lang.oo.general");
(function(){
var oo=_3.lang.oo,md=oo.makeDecorator,_4=oo.general,_5=oo.aop,_6=_1.isFunction;
_5.before=_4.before;
_5.around=_4.wrap;
_5.afterReturning=md(function(_7,_8,_9){
return _6(_9)?function(){
var _a=_9.apply(this,arguments);
_8.call(this,_a);
return _a;
}:function(){
_8.call(this);
};
});
_5.afterThrowing=md(function(_b,_c,_d){
return _6(_d)?function(){
var _e;
try{
_e=_d.apply(this,arguments);
}
catch(e){
_c.call(this,e);
throw e;
}
return _e;
}:_d;
});
_5.after=md(function(_f,_10,_11){
return _6(_11)?function(){
var ret;
try{
ret=_11.apply(this,arguments);
}
finally{
_10.call(this);
}
return ret;
}:function(){
_10.call(this);
};
});
})();
});
