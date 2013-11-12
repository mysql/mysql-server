//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.util.positioning");
(function(){
var _4=4;
var _5=20;
_3.drawing.util.positioning.label=function(_6,_7){
var x=0.5*(_6.x+_7.x);
var y=0.5*(_6.y+_7.y);
var _8=_3.drawing.util.common.slope(_6,_7);
var _9=_4/Math.sqrt(1+_8*_8);
if(_7.y>_6.y&&_7.x>_6.x||_7.y<_6.y&&_7.x<_6.x){
_9=-_9;
y-=_5;
}
x+=-_9*_8;
y+=_9;
var _a=_7.x<_6.x?"end":"start";
return {x:x,y:y,foo:"bar",align:_a};
};
_3.drawing.util.positioning.angle=function(_b,_c){
var x=0.7*_b.x+0.3*_c.x;
var y=0.7*_b.y+0.3*_c.y;
var _d=_3.drawing.util.common.slope(_b,_c);
var _e=_4/Math.sqrt(1+_d*_d);
if(_c.x<_b.x){
_e=-_e;
}
x+=-_e*_d;
y+=_e;
var _f=_c.y>_b.y?"end":"start";
y+=_c.x>_b.x?0.5*_5:-0.5*_5;
return {x:x,y:y,align:_f};
};
})();
});
