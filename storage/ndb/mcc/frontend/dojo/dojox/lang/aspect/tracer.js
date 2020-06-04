//>>built
define("dojox/lang/aspect/tracer",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect.tracer");
(function(){
var _4=_3.lang.aspect;
var _5=function(_6){
this.method=_6?"group":"log";
if(_6){
this.after=this._after;
}
};
_1.extend(_5,{before:function(){
var _7=_4.getContext(),_8=_7.joinPoint,_9=Array.prototype.join.call(arguments,", ");
console[this.method](_7.instance,"=>",_8.targetName+"("+_9+")");
},afterReturning:function(_a){
var _b=_4.getContext().joinPoint;
if(typeof _a!="undefined"){
}else{
}
},afterThrowing:function(_c){
},_after:function(_d){
}});
_4.tracer=function(_e){
return new _5(_e);
};
})();
});
