//>>built
define("dojox/lang/async/topic",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.async.topic");
(function(){
var d=_1,_4=_3.lang.async.topic;
_4.from=function(_5){
return function(){
var h,_6=function(){
if(h){
d.unsubscribe(h);
h=null;
}
},x=new d.Deferred(_6);
h=d.subscribe(_5,function(){
_6();
x.callback(arguments);
});
return x;
};
};
_4.failOn=function(_7){
return function(){
var h,_8=function(){
if(h){
d.unsubscribe(h);
h=null;
}
},x=new d.Deferred(_8);
h=d.subscribe(_7,function(_9){
_8();
x.errback(new Error(arguments));
});
return x;
};
};
})();
});
