//>>built
define("dojox/lang/oo/Decorator",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.oo.Decorator");
(function(){
var oo=_3.lang.oo,D=oo.Decorator=function(_4,_5){
this.value=_4;
this.decorator=typeof _5=="object"?function(){
return _5.exec.apply(_5,arguments);
}:_5;
};
oo.makeDecorator=function(_6){
return function(_7){
return new D(_7,_6);
};
};
})();
});
