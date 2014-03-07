//>>built
define("dojox/charting/scaler/linear",["dojo/_base/lang","./common"],function(_1,_2){
var _3=_1.getObject("dojox.charting.scaler.linear",true);
var _4=3,_5=_2.findString,_6=_2.getNumericLabel;
var _7=function(_8,_9,_a,_b,_c,_d,_e){
_a=_1.delegate(_a);
if(!_b){
if(_a.fixUpper=="major"){
_a.fixUpper="minor";
}
if(_a.fixLower=="major"){
_a.fixLower="minor";
}
}
if(!_c){
if(_a.fixUpper=="minor"){
_a.fixUpper="micro";
}
if(_a.fixLower=="minor"){
_a.fixLower="micro";
}
}
if(!_d){
if(_a.fixUpper=="micro"){
_a.fixUpper="none";
}
if(_a.fixLower=="micro"){
_a.fixLower="none";
}
}
var _f=_5(_a.fixLower,["major"])?Math.floor(_a.min/_b)*_b:_5(_a.fixLower,["minor"])?Math.floor(_a.min/_c)*_c:_5(_a.fixLower,["micro"])?Math.floor(_a.min/_d)*_d:_a.min,_10=_5(_a.fixUpper,["major"])?Math.ceil(_a.max/_b)*_b:_5(_a.fixUpper,["minor"])?Math.ceil(_a.max/_c)*_c:_5(_a.fixUpper,["micro"])?Math.ceil(_a.max/_d)*_d:_a.max;
if(_a.useMin){
_8=_f;
}
if(_a.useMax){
_9=_10;
}
var _11=(!_b||_a.useMin&&_5(_a.fixLower,["major"]))?_8:Math.ceil(_8/_b)*_b,_12=(!_c||_a.useMin&&_5(_a.fixLower,["major","minor"]))?_8:Math.ceil(_8/_c)*_c,_13=(!_d||_a.useMin&&_5(_a.fixLower,["major","minor","micro"]))?_8:Math.ceil(_8/_d)*_d,_14=!_b?0:(_a.useMax&&_5(_a.fixUpper,["major"])?Math.round((_9-_11)/_b):Math.floor((_9-_11)/_b))+1,_15=!_c?0:(_a.useMax&&_5(_a.fixUpper,["major","minor"])?Math.round((_9-_12)/_c):Math.floor((_9-_12)/_c))+1,_16=!_d?0:(_a.useMax&&_5(_a.fixUpper,["major","minor","micro"])?Math.round((_9-_13)/_d):Math.floor((_9-_13)/_d))+1,_17=_c?Math.round(_b/_c):0,_18=_d?Math.round(_c/_d):0,_19=_b?Math.floor(Math.log(_b)/Math.LN10):0,_1a=_c?Math.floor(Math.log(_c)/Math.LN10):0,_1b=_e/(_9-_8);
if(!isFinite(_1b)){
_1b=1;
}
return {bounds:{lower:_f,upper:_10,from:_8,to:_9,scale:_1b,span:_e},major:{tick:_b,start:_11,count:_14,prec:_19},minor:{tick:_c,start:_12,count:_15,prec:_1a},micro:{tick:_d,start:_13,count:_16,prec:0},minorPerMajor:_17,microPerMinor:_18,scaler:_3};
};
return _1.mixin(_3,{buildScaler:function(min,max,_1c,_1d){
var h={fixUpper:"none",fixLower:"none",natural:false};
if(_1d){
if("fixUpper" in _1d){
h.fixUpper=String(_1d.fixUpper);
}
if("fixLower" in _1d){
h.fixLower=String(_1d.fixLower);
}
if("natural" in _1d){
h.natural=Boolean(_1d.natural);
}
}
if("min" in _1d){
min=_1d.min;
}
if("max" in _1d){
max=_1d.max;
}
if(_1d.includeZero){
if(min>0){
min=0;
}
if(max<0){
max=0;
}
}
h.min=min;
h.useMin=true;
h.max=max;
h.useMax=true;
if("from" in _1d){
min=_1d.from;
h.useMin=false;
}
if("to" in _1d){
max=_1d.to;
h.useMax=false;
}
if(max<=min){
return _7(min,max,h,0,0,0,_1c);
}
var mag=Math.floor(Math.log(max-min)/Math.LN10),_1e=_1d&&("majorTickStep" in _1d)?_1d.majorTickStep:Math.pow(10,mag),_1f=0,_20=0,_21;
if(_1d&&("minorTickStep" in _1d)){
_1f=_1d.minorTickStep;
}else{
do{
_1f=_1e/10;
if(!h.natural||_1f>0.9){
_21=_7(min,max,h,_1e,_1f,0,_1c);
if(_21.bounds.scale*_21.minor.tick>_4){
break;
}
}
_1f=_1e/5;
if(!h.natural||_1f>0.9){
_21=_7(min,max,h,_1e,_1f,0,_1c);
if(_21.bounds.scale*_21.minor.tick>_4){
break;
}
}
_1f=_1e/2;
if(!h.natural||_1f>0.9){
_21=_7(min,max,h,_1e,_1f,0,_1c);
if(_21.bounds.scale*_21.minor.tick>_4){
break;
}
}
return _7(min,max,h,_1e,0,0,_1c);
}while(false);
}
if(_1d&&("microTickStep" in _1d)){
_20=_1d.microTickStep;
_21=_7(min,max,h,_1e,_1f,_20,_1c);
}else{
do{
_20=_1f/10;
if(!h.natural||_20>0.9){
_21=_7(min,max,h,_1e,_1f,_20,_1c);
if(_21.bounds.scale*_21.micro.tick>_4){
break;
}
}
_20=_1f/5;
if(!h.natural||_20>0.9){
_21=_7(min,max,h,_1e,_1f,_20,_1c);
if(_21.bounds.scale*_21.micro.tick>_4){
break;
}
}
_20=_1f/2;
if(!h.natural||_20>0.9){
_21=_7(min,max,h,_1e,_1f,_20,_1c);
if(_21.bounds.scale*_21.micro.tick>_4){
break;
}
}
_20=0;
}while(false);
}
return _20?_21:_7(min,max,h,_1e,_1f,0,_1c);
},buildTicks:function(_22,_23){
var _24,_25,_26,_27=_22.major.start,_28=_22.minor.start,_29=_22.micro.start;
if(_23.microTicks&&_22.micro.tick){
_24=_22.micro.tick,_25=_29;
}else{
if(_23.minorTicks&&_22.minor.tick){
_24=_22.minor.tick,_25=_28;
}else{
if(_22.major.tick){
_24=_22.major.tick,_25=_27;
}else{
return null;
}
}
}
var _2a=1/_22.bounds.scale;
if(_22.bounds.to<=_22.bounds.from||isNaN(_2a)||!isFinite(_2a)||_24<=0||isNaN(_24)||!isFinite(_24)){
return null;
}
var _2b=[],_2c=[],_2d=[];
while(_25<=_22.bounds.to+_2a){
if(Math.abs(_27-_25)<_24/2){
_26={value:_27};
if(_23.majorLabels){
_26.label=_6(_27,_22.major.prec,_23);
}
_2b.push(_26);
_27+=_22.major.tick;
_28+=_22.minor.tick;
_29+=_22.micro.tick;
}else{
if(Math.abs(_28-_25)<_24/2){
if(_23.minorTicks){
_26={value:_28};
if(_23.minorLabels&&(_22.minMinorStep<=_22.minor.tick*_22.bounds.scale)){
_26.label=_6(_28,_22.minor.prec,_23);
}
_2c.push(_26);
}
_28+=_22.minor.tick;
_29+=_22.micro.tick;
}else{
if(_23.microTicks){
_2d.push({value:_29});
}
_29+=_22.micro.tick;
}
}
_25+=_24;
}
return {major:_2b,minor:_2c,micro:_2d};
},getTransformerFromModel:function(_2e){
var _2f=_2e.bounds.from,_30=_2e.bounds.scale;
return function(x){
return (x-_2f)*_30;
};
},getTransformerFromPlot:function(_31){
var _32=_31.bounds.from,_33=_31.bounds.scale;
return function(x){
return x/_33+_32;
};
}});
});
