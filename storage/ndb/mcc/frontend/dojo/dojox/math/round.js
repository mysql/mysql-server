//>>built
define("dojox/math/round",["dojo","dojox"],function(_1,_2){
_1.getObject("math.round",true,_2);
_1.experimental("dojox.math.round");
_2.math.round=function(_3,_4,_5){
var _6=Math.log(Math.abs(_3))/Math.log(10);
var _7=10/(_5||10);
var _8=Math.pow(10,-15+_6);
return (_7*(+_3+(_3>0?_8:-_8))).toFixed(_4)/_7;
};
if((0.9).toFixed()==0){
var _9=_2.math.round;
_2.math.round=function(v,p,m){
var d=Math.pow(10,-p||0),a=Math.abs(v);
if(!v||a>=d){
d=0;
}else{
a/=d;
if(a<0.5||a>=0.95){
d=0;
}
}
return _9(v,p,m)+(v>0?d:-d);
};
}
return _2.math.round;
});
