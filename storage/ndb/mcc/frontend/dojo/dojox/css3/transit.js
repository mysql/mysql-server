//>>built
define("dojox/css3/transit",["dojo/_base/kernel","dojo/_base/array","dojo/dom-style","dojo/DeferredList","./transition"],function(_1,_2,_3,_4,_5){
var _6=function(_7,to,_8){
var _9=(_8&&_8.reverse)?-1:1;
if(!_8||!_8.transition||!_5[_8.transition]){
_3.set(_7,"display","none");
_3.set(to,"display","");
if(_8.transitionDefs){
if(_8.transitionDefs[_7.id]){
_8.transitionDefs[_7.id].resolve(_7);
}
if(_8.transitionDefs[to.id]){
_8.transitionDefs[to.id].resolve(to);
}
}
}else{
var _a=[];
var _b=[];
var _c=250;
if(_8.transition==="fade"){
_c=600;
}else{
if(_8.transition==="flip"){
_c=200;
}
}
_3.set(_7,"display","");
_3.set(to,"display","");
if(_7){
var _d=_5[_8.transition](_7,{"in":false,direction:_9,duration:_c,deferred:(_8.transitionDefs&&_8.transitionDefs[_7.id])?_8.transitionDefs[_7.id]:null});
_a.push(_d.deferred);
_b.push(_d);
}
var _e=_5[_8.transition](to,{direction:_9,duration:_c,deferred:(_8.transitionDefs&&_8.transitionDefs[to.id])?_8.transitionDefs[to.id]:null});
_a.push(_e.deferred);
_b.push(_e);
if(_8.transition==="flip"){
_5.chainedPlay(_b);
}else{
_5.groupedPlay(_b);
}
return new _4(_a);
}
};
return _6;
});
