//>>built
define("dojox/app/transition",["dojo/_base/kernel","dojo/_base/array","dojo/_base/html","dojo/DeferredList","./animation"],function(_1,_2,_3,_4,_5){
return function(_6,to,_7){
var _8=(_7&&_7.reverse)?-1:1;
if(!_7||!_7.transition||!_5[_7.transition]){
_1.style(_6,"display","none");
_1.style(to,"display","");
if(_7.transitionDefs){
if(_7.transitionDefs[_6.id]){
_7.transitionDefs[_6.id].resolve(_6);
}
if(_7.transitionDefs[to.id]){
_7.transitionDefs[to.id].resolve(to);
}
}
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
_1.style(_6,"display","");
_1.style(to,"display","");
if(_6){
var _c=_5[_7.transition](_6,{"in":false,direction:_8,duration:_b,deferred:(_7.transitionDefs&&_7.transitionDefs[_6.id])?_7.transitionDefs[_6.id]:null});
_9.push(_c.deferred);
_a.push(_c);
}
var _d=_5[_7.transition](to,{direction:_8,duration:_b,deferred:(_7.transitionDefs&&_7.transitionDefs[to.id])?_7.transitionDefs[to.id]:null});
_9.push(_d.deferred);
_a.push(_d);
if(_7.transition==="flip"){
_5.chainedPlay(_a);
}else{
_5.groupedPlay(_a);
}
return new _1.DeferredList(_9);
}
};
});
