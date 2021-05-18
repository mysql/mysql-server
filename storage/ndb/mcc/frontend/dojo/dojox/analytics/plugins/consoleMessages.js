//>>built
define("dojox/analytics/plugins/consoleMessages",["dojo/_base/lang","../_base","dojo/_base/config","dojo/aspect"],function(_1,_2,_3,_4){
var _5=_1.getObject("dojox.analytics.plugins.consoleMessages",true);
_5.addData=_1.hitch(_2,"addData","consoleMessages");
var _6=_3["consoleLogFuncs"]||["error","warn","info","rlog"];
if(!console){
console={};
}
for(var i=0;i<_6.length;i++){
var _7=_6[i],_8=_1.hitch(_5,"addData",_7);
if(console[_7]){
_4.after(console,_7,_8,true);
}else{
console[_7]=_8;
}
}
return _5;
});
