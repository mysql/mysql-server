//>>built
define("dojox/lang/aspect/counter",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect.counter");
(function(){
var _4=_3.lang.aspect;
var _5=function(){
this.reset();
};
_1.extend(_5,{before:function(){
++this.calls;
},afterThrowing:function(){
++this.errors;
},reset:function(){
this.calls=this.errors=0;
}});
_4.counter=function(){
return new _5;
};
})();
});
