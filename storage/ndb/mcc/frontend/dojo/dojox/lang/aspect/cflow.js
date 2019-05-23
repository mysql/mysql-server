//>>built
define("dojox/lang/aspect/cflow",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect.cflow");
(function(){
var _4=_3.lang.aspect;
_4.cflow=function(_5,_6){
if(arguments.length>1&&!(_6 instanceof Array)){
_6=[_6];
}
var _7=_4.getContextStack();
for(var i=_7.length-1;i>=0;--i){
var c=_7[i];
if(_5&&c.instance!=_5){
continue;
}
if(!_6){
return true;
}
var n=c.joinPoint.targetName;
for(var j=_6.length-1;j>=0;--j){
var m=_6[j];
if(m instanceof RegExp){
if(m.test(n)){
return true;
}
}else{
if(n==m){
return true;
}
}
}
}
return false;
};
})();
});
