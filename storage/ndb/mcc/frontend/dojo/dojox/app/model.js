//>>built
define("dojox/app/model",["dojo/_base/kernel","dojo/_base/Deferred","dojox/mvc/StatefulModel"],function(_1,_2){
return function(_3,_4){
var _5={};
if(_4){
_1.mixin(_5,_4);
}
if(_3){
for(var _6 in _3){
if(_6.charAt(0)!=="_"){
var _7=_3[_6].params?_3[_6].params:{};
var _8={"store":_7.store.store,"query":_7.store.query?_7.store.query:{}};
_5[_6]=_2.when(dojox.mvc.newStatefulModel(_8),function(_9){
return _9;
});
}
}
}
return _5;
};
});
