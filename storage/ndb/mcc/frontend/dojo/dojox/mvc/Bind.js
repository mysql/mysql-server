//>>built
define("dojox/mvc/Bind",["dojo/_base/lang","dojo/_base/array"],function(_1,_2){
var _3=_1.getObject("dojox.mvc",true);
return _1.mixin(_3,{bind:function(_4,_5,_6,_7,_8,_9){
var _a;
return _4.watch(_5,function(_b,_c,_d){
_a=_1.isFunction(_8)?_8(_d):_d;
if(!_9||_a!=_6.get(_7)){
_6.set(_7,_a);
}
});
},bindInputs:function(_e,_f){
var _10=[];
_2.forEach(_e,function(h){
_10.push(h.watch("value",_f));
});
return _10;
}});
});
