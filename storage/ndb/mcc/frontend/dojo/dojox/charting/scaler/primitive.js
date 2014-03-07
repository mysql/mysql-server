//>>built
define("dojox/charting/scaler/primitive",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.charting.scaler.primitive",true);
return _1.mixin(_2,{buildScaler:function(_3,_4,_5,_6){
if(_3==_4){
_3-=0.5;
_4+=0.5;
}
return {bounds:{lower:_3,upper:_4,from:_3,to:_4,scale:_5/(_4-_3),span:_5},scaler:_2};
},buildTicks:function(_7,_8){
return {major:[],minor:[],micro:[]};
},getTransformerFromModel:function(_9){
var _a=_9.bounds.from,_b=_9.bounds.scale;
return function(x){
return (x-_a)*_b;
};
},getTransformerFromPlot:function(_c){
var _d=_c.bounds.from,_e=_c.bounds.scale;
return function(x){
return x/_e+_d;
};
}});
});
