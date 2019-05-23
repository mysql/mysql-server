//>>built
define("dojox/lang/async/timeout",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.async.timeout");
(function(){
var d=_1,_4=_3.lang.async.timeout;
_4.from=function(ms){
return function(){
var h,_5=function(){
if(h){
clearTimeout(h);
h=null;
}
},x=new d.Deferred(_5);
h=setTimeout(function(){
_5();
x.callback(ms);
},ms);
return x;
};
};
_4.failOn=function(ms){
return function(){
var h,_6=function(){
if(h){
clearTimeout(h);
h=null;
}
},x=new d.Deferred(_6);
h=setTimeout(function(){
_6();
x.errback(ms);
},ms);
return x;
};
};
})();
});
