//>>built
define("dojox/drawing/util/positioning",["./common"],function(_1){
var _2=4;
var _3=20;
var _4={};
_4.label=function(_5,_6){
var x=0.5*(_5.x+_6.x);
var y=0.5*(_5.y+_6.y);
var _7=_1.slope(_5,_6);
var _8=_2/Math.sqrt(1+_7*_7);
if(_6.y>_5.y&&_6.x>_5.x||_6.y<_5.y&&_6.x<_5.x){
_8=-_8;
y-=_3;
}
x+=-_8*_7;
y+=_8;
var _9=_6.x<_5.x?"end":"start";
return {x:x,y:y,foo:"bar",align:_9};
};
_4.angle=function(_a,_b){
var x=0.7*_a.x+0.3*_b.x;
var y=0.7*_a.y+0.3*_b.y;
var _c=_1.slope(_a,_b);
var _d=_2/Math.sqrt(1+_c*_c);
if(_b.x<_a.x){
_d=-_d;
}
x+=-_d*_c;
y+=_d;
var _e=_b.y>_a.y?"end":"start";
y+=_b.x>_a.x?0.5*_3:-0.5*_3;
return {x:x,y:y,align:_e};
};
return _4;
});
