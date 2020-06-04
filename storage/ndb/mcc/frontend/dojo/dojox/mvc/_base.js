//>>built
define("dojox/mvc/_base",["dojo/_base/kernel","dojo/_base/lang","./getStateful","./StatefulModel","./Bind","./_DataBindingMixin","./_patches"],function(_1,_2,_3,_4){
_1.experimental("dojox.mvc");
var _5=_2.getObject("dojox.mvc",true);
_5.newStatefulModel=function(_6){
if(_6.data){
return _3(_6.data,_4.getStatefulOptions);
}else{
if(_6.store&&_2.isFunction(_6.store.query)){
var _7;
var _8=_6.store.query(_6.query);
if(_8.then){
return (_8.then(function(_9){
_7=_3(_9,_4.getStatefulOptions);
_7.store=_6.store;
return _7;
}));
}else{
_7=_3(_8,_4.getStatefulOptions);
_7.store=_6.store;
return _7;
}
}
}
};
return _5;
});
