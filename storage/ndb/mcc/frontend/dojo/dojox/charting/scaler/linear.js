//>>built
define("dojox/charting/scaler/linear",["dojo/_base/lang","./common"],function(_1,_2){
var _3=_1.getObject("dojox.charting.scaler.linear",true);
var _4=3,_5=_2.getNumericLabel;
function _6(_7,_8){
_7=_7.toLowerCase();
for(var i=_8.length-1;i>=0;--i){
if(_7===_8[i]){
return true;
}
}
return false;
};
var _9=function(_a,_b,_c,_d,_e,_f,_10){
_c=_1.delegate(_c);
if(!_d){
if(_c.fixUpper=="major"){
_c.fixUpper="minor";
}
if(_c.fixLower=="major"){
_c.fixLower="minor";
}
}
if(!_e){
if(_c.fixUpper=="minor"){
_c.fixUpper="micro";
}
if(_c.fixLower=="minor"){
_c.fixLower="micro";
}
}
if(!_f){
if(_c.fixUpper=="micro"){
_c.fixUpper="none";
}
if(_c.fixLower=="micro"){
_c.fixLower="none";
}
}
var _11=_6(_c.fixLower,["major"])?Math.floor(_c.min/_d)*_d:_6(_c.fixLower,["minor"])?Math.floor(_c.min/_e)*_e:_6(_c.fixLower,["micro"])?Math.floor(_c.min/_f)*_f:_c.min,_12=_6(_c.fixUpper,["major"])?Math.ceil(_c.max/_d)*_d:_6(_c.fixUpper,["minor"])?Math.ceil(_c.max/_e)*_e:_6(_c.fixUpper,["micro"])?Math.ceil(_c.max/_f)*_f:_c.max;
if(_c.useMin){
_a=_11;
}
if(_c.useMax){
_b=_12;
}
var _13=(!_d||_c.useMin&&_6(_c.fixLower,["major"]))?_a:Math.ceil(_a/_d)*_d,_14=(!_e||_c.useMin&&_6(_c.fixLower,["major","minor"]))?_a:Math.ceil(_a/_e)*_e,_15=(!_f||_c.useMin&&_6(_c.fixLower,["major","minor","micro"]))?_a:Math.ceil(_a/_f)*_f,_16=!_d?0:(_c.useMax&&_6(_c.fixUpper,["major"])?Math.round((_b-_13)/_d):Math.floor((_b-_13)/_d))+1,_17=!_e?0:(_c.useMax&&_6(_c.fixUpper,["major","minor"])?Math.round((_b-_14)/_e):Math.floor((_b-_14)/_e))+1,_18=!_f?0:(_c.useMax&&_6(_c.fixUpper,["major","minor","micro"])?Math.round((_b-_15)/_f):Math.floor((_b-_15)/_f))+1,_19=_e?Math.round(_d/_e):0,_1a=_f?Math.round(_e/_f):0,_1b=_d?Math.floor(Math.log(_d)/Math.LN10):0,_1c=_e?Math.floor(Math.log(_e)/Math.LN10):0,_1d=_10/(_b-_a);
if(!isFinite(_1d)){
_1d=1;
}
return {bounds:{lower:_11,upper:_12,from:_a,to:_b,scale:_1d,span:_10},major:{tick:_d,start:_13,count:_16,prec:_1b},minor:{tick:_e,start:_14,count:_17,prec:_1c},micro:{tick:_f,start:_15,count:_18,prec:0},minorPerMajor:_19,microPerMinor:_1a,scaler:_3};
};
return _1.mixin(_3,{buildScaler:function(min,max,_1e,_1f,_20,_21){
var h={fixUpper:"none",fixLower:"none",natural:false};
if(_1f){
if("fixUpper" in _1f){
h.fixUpper=String(_1f.fixUpper);
}
if("fixLower" in _1f){
h.fixLower=String(_1f.fixLower);
}
if("natural" in _1f){
h.natural=Boolean(_1f.natural);
}
}
_21=!_21||_21<_4?_4:_21;
if("min" in _1f){
min=_1f.min;
}
if("max" in _1f){
max=_1f.max;
}
if(_1f.includeZero){
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
if("from" in _1f){
min=_1f.from;
h.useMin=false;
}
if("to" in _1f){
max=_1f.to;
h.useMax=false;
}
if(max<=min){
return _9(min,max,h,0,0,0,_1e);
}
if(!_20){
_20=max-min;
}
var mag=Math.floor(Math.log(_20)/Math.LN10),_22=_1f&&("majorTickStep" in _1f)?_1f.majorTickStep:Math.pow(10,mag),_23=0,_24=0,_25;
if(_1f&&("minorTickStep" in _1f)){
_23=_1f.minorTickStep;
}else{
do{
_23=_22/10;
if(!h.natural||_23>0.9){
_25=_9(min,max,h,_22,_23,0,_1e);
if(_25.bounds.scale*_25.minor.tick>_21){
break;
}
}
_23=_22/5;
if(!h.natural||_23>0.9){
_25=_9(min,max,h,_22,_23,0,_1e);
if(_25.bounds.scale*_25.minor.tick>_21){
break;
}
}
_23=_22/2;
if(!h.natural||_23>0.9){
_25=_9(min,max,h,_22,_23,0,_1e);
if(_25.bounds.scale*_25.minor.tick>_21){
break;
}
}
return _9(min,max,h,_22,0,0,_1e);
}while(false);
}
if(_1f&&("microTickStep" in _1f)){
_24=_1f.microTickStep;
_25=_9(min,max,h,_22,_23,_24,_1e);
}else{
do{
_24=_23/10;
if(!h.natural||_24>0.9){
_25=_9(min,max,h,_22,_23,_24,_1e);
if(_25.bounds.scale*_25.micro.tick>_4){
break;
}
}
_24=_23/5;
if(!h.natural||_24>0.9){
_25=_9(min,max,h,_22,_23,_24,_1e);
if(_25.bounds.scale*_25.micro.tick>_4){
break;
}
}
_24=_23/2;
if(!h.natural||_24>0.9){
_25=_9(min,max,h,_22,_23,_24,_1e);
if(_25.bounds.scale*_25.micro.tick>_4){
break;
}
}
_24=0;
}while(false);
}
return _24?_25:_9(min,max,h,_22,_23,0,_1e);
},buildTicks:function(_26,_27){
var _28,_29,_2a,_2b=_26.major.start,_2c=_26.minor.start,_2d=_26.micro.start;
if(_27.microTicks&&_26.micro.tick){
_28=_26.micro.tick,_29=_2d;
}else{
if(_27.minorTicks&&_26.minor.tick){
_28=_26.minor.tick,_29=_2c;
}else{
if(_26.major.tick){
_28=_26.major.tick,_29=_2b;
}else{
return null;
}
}
}
var _2e=1/_26.bounds.scale;
if(_26.bounds.to<=_26.bounds.from||isNaN(_2e)||!isFinite(_2e)||_28<=0||isNaN(_28)||!isFinite(_28)){
return null;
}
var _2f=[],_30=[],_31=[];
while(_29<=_26.bounds.to+_2e){
if(Math.abs(_2b-_29)<_28/2){
_2a={value:_2b};
if(_27.majorLabels){
_2a.label=_5(_2b,_26.major.prec,_27);
}
_2f.push(_2a);
_2b+=_26.major.tick;
_2c+=_26.minor.tick;
_2d+=_26.micro.tick;
}else{
if(Math.abs(_2c-_29)<_28/2){
if(_27.minorTicks){
_2a={value:_2c};
if(_27.minorLabels&&(_26.minMinorStep<=_26.minor.tick*_26.bounds.scale)){
_2a.label=_5(_2c,_26.minor.prec,_27);
}
_30.push(_2a);
}
_2c+=_26.minor.tick;
_2d+=_26.micro.tick;
}else{
if(_27.microTicks){
_31.push({value:_2d});
}
_2d+=_26.micro.tick;
}
}
_29+=_28;
}
return {major:_2f,minor:_30,micro:_31};
},getTransformerFromModel:function(_32){
var _33=_32.bounds.from,_34=_32.bounds.scale;
return function(x){
return (x-_33)*_34;
};
},getTransformerFromPlot:function(_35){
var _36=_35.bounds.from,_37=_35.bounds.scale;
return function(x){
return x/_37+_36;
};
}});
});
