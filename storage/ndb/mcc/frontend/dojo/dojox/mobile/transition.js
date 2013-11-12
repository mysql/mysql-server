//>>built
define("dojox/mobile/transition",["dojo/_base/Deferred","dojo/_base/config"],function(_1,_2){
if(_2["mblCSS3Transition"]){
var _3=new _1();
require([_2["mblCSS3Transition"]],function(_4){
_3.resolve(_4);
});
return _3;
}
return null;
});
