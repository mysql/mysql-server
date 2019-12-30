//>>built
define("dojox/color/MeanColorModel",["dojo/_base/array","dojo/_base/declare","./NeutralColorModel"],function(_1,_2,_3){
return _2("dojox.color.MeanColorModel",_3,{constructor:function(_4,_5){
},computeNeutral:function(_6,_7,_8,_9){
var _a=_6;
if(_9.length!=0){
if(_9.length<3){
_a=_8/_9.length;
}else{
if((_9.length&1)==0){
_a=(_9[_9.length/2-1]+_9[_9.length/2])/2;
}else{
_a=_9[Math.floor(_9.length/2)];
}
}
}
return _a;
}});
});
