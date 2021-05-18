//>>built
define("dojox/dgauges/_circularUtils",function(){
return {computeTotalAngle:function(_1,_2,_3){
if(_1==_2){
return 360;
}else{
return this.computeAngle(_1,_2,_3,360);
}
},modAngle:function(_4,_5){
if(_5==undefined){
_5=6.28318530718;
}
if(_4>=_5){
do{
_4-=_5;
}while(_4>=_5);
}else{
while(_4<0){
_4+=_5;
}
}
return _4;
},computeAngle:function(_6,_7,_8,_9){
if(_9==undefined){
_9=6.28318530718;
}
var _a;
if(_7==_6){
return _9;
}
if(_8=="clockwise"){
if(_7<_6){
_a=_9-(_6-_7);
}else{
_a=_7-_6;
}
}else{
if(_7<_6){
_a=_6-_7;
}else{
_a=_9-(_7-_6);
}
}
return this.modAngle(_a,_9);
},toRadians:function(_b){
return _b*Math.PI/180;
},toDegrees:function(_c){
return _c*180/Math.PI;
}};
});
