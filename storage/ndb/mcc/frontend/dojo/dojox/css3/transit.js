//>>built
define("dojox/css3/transit",["dojo/_base/array","dojo/dom-style","dojo/DeferredList","./transition"],function(_1,_2,_3,_4){
var _5=function(_6,to,_7){
var _8=(_7&&_7.reverse)?-1:1;
if(!_7||!_7.transition||!_4[_7.transition]){
_2.set(_6,"display","none");
_2.set(to,"display","");
if(_7.transitionDefs){
if(_7.transitionDefs[_6.id]){
_7.transitionDefs[_6.id].resolve(_6);
}
if(_7.transitionDefs[to.id]){
_7.transitionDefs[to.id].resolve(to);
}
}
return new _3([]);
}else{
var _9=[];
var _a=[];
var _b=250;
if(_7.transition==="fade"){
_b=600;
}else{
if(_7.transition==="flip"){
_b=200;
}
}
_2.set(_6,"display","");
_2.set(to,"display","");
if(_6){
var _c=_4[_7.transition](_6,{"in":false,direction:_8,duration:_b,deferred:(_7.transitionDefs&&_7.transitionDefs[_6.id])?_7.transitionDefs[_6.id]:null});
_9.push(_c.deferred);
_a.push(_c);
}
var _d=_4[_7.transition](to,{direction:_8,duration:_b,deferred:(_7.transitionDefs&&_7.transitionDefs[to.id])?_7.transitionDefs[to.id]:null});
_9.push(_d.deferred);
_a.push(_d);
if(_7.transition==="flip"){
_4.chainedPlay(_a);
}else{
_4.groupedPlay(_a);
}
return new _3(_9);
}
};
return _5;
});
