//>>built
define("dojox/analytics/plugins/consoleMessages",["dojo/_base/lang","../_base","dojo/_base/config","dojo/aspect"],function(_1,_2,_3,_4){
consoleMessages=_1.getObject("dojox.analytics.plugins.consoleMessages",true);
this.addData=_1.hitch(_2,"addData","consoleMessages");
var _5=_3["consoleLogFuncs"]||["error","warn","info","rlog"];
if(!console){
console={};
}
for(var i=0;i<_5.length;i++){
if(console[_5[i]]){
_4.after(console,_5[i],_1.hitch(this,"addData",_5[i]),true);
}else{
console[_5[i]]=_1.hitch(this,"addData",_5[i]);
}
}
return consoleMessages;
});
