//>>built
define("dojox/timing/doLater",["./_base"],function(_1){
dojo.experimental("dojox.timing.doLater");
_1.doLater=function(_2,_3,_4){
if(_2){
return false;
}
var _5=_1.doLater.caller,_6=_1.doLater.caller.arguments;
_4=_4||100;
_3=_3||dojo.global;
setTimeout(function(){
_5.apply(_3,_6);
},_4);
return true;
};
return _1.doLater;
});
