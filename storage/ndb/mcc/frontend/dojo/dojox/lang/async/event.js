//>>built
define("dojox/lang/async/event",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.async.event");
(function(){
var d=_1,_4=_3.lang.async.event;
_4.from=function(_5,_6){
return function(){
var h,_7=function(){
if(h){
d.disconnect(h);
h=null;
}
},x=new d.Deferred(_7);
h=d.connect(_5,_6,function(_8){
_7();
x.callback(_8);
});
return x;
};
};
_4.failOn=function(_9,_a){
return function(){
var h,_b=function(){
if(h){
d.disconnect(h);
h=null;
}
},x=new d.Deferred(_b);
h=d.connect(_9,_a,function(_c){
_b();
x.errback(new Error(_c));
});
return x;
};
};
})();
});
