//>>built
define("dojox/charting/scaler/log",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","./linear","./common"],function(_1,_2,_3,_4,_5){
var _6={},_7=_5.getNumericLabel;
function _8(_9,_a){
var _b=0;
if(_9<0.6){
var m=_9.toString().match(/\.(\d+)/);
if(m&&m[1]){
_b=-m[1].length;
}
}
return _7(_9,_b,_a);
};
return _2.mixin(_6,_4,{base:10,setBase:function(_c){
this.base=Math.round(_c||10);
},buildScaler:function(_d,_e,_f,_10){
var _11=Math.log(this.base),_12,to;
if("min" in _10){
_d=_10.min;
}
if("max" in _10){
_e=_10.max;
}
_d=Math.log(_d)/_11;
_e=Math.log(_e)/_11;
var _13=Math.floor(_d),_14=Math.ceil(_e);
_d=_13<_d?_13:_d-1;
_e=_e<_14?_14:_e+1;
if(_10.includeZero){
if(_d>0){
_d=0;
}
if(_e<0){
_e=0;
}
if(_12>0){
_12=0;
}
if(to>0){
to=0;
}
}
var _15={min:_d,max:_e,fixUpper:_10.fixUpper,fixLower:_10.fixLower,natural:_10.natural,minorTicks:false,minorLabels:false,majorTickStep:1};
if("from" in _10){
_15.from=Math.log(_10.from)/_11;
}
if("to" in _10){
_15.to=Math.log(_10.to)/_11;
}
var _16=_4.buildScaler.call(_4,_d,_e,_f,_15);
_16.scaler=_6;
_16.bounds.lower=Math.exp(_16.bounds.lower*_11);
_16.bounds.upper=Math.exp(_16.bounds.upper*_11);
_16.bounds.from=Math.exp(_16.bounds.from*_11);
_16.bounds.to=Math.exp(_16.bounds.to*_11);
return _16;
},buildTicks:function(_17,_18){
var _19=this.base,_1a=Math.log(this.base);
var _1b=_2.mixin({},_17.bounds);
_17.bounds.lower=Math.log(_17.bounds.lower)/_1a;
_17.bounds.upper=Math.log(_17.bounds.upper)/_1a;
_17.bounds.from=Math.log(_17.bounds.from)/_1a;
_17.bounds.to=Math.log(_17.bounds.to)/_1a;
var _1c=_2.mixin({},_18);
_1c.minorTicks=_1c.minorLabels=false;
_1c.majorTickStep=1;
var _1d=_4.buildTicks.call(_4,_17,_1c);
_2.mixin(_17.bounds,_1b);
if(!_1d){
return _1d;
}
function _1e(_1f){
_1f.value=Math.exp(_1f.value*_1a);
if(_1f.value>=1){
_1f.value=Math.round(_1f.value);
}
if(_19===10){
_1f.value=+_1f.value.toPrecision(1);
}
if(_18.minorLabels){
_1f.label=_8(_1f.value,_18);
}
};
_3.forEach(_1d.major,_1e);
_1d.minor=[];
if(_18.minorTicks&&this.base===10){
var _20=_17.bounds.from,to=_17.bounds.to,_21=function(_22){
if(_20<=_22&&_22<=to){
if(_18.minorLabels){
_1d.minor.push({value:_22,label:_8(_22,_18)});
}else{
_1d.minor.push({value:_22});
}
}
};
if(_1d.major.length){
_21(+(_1d.major[0].value/5).toPrecision(1));
_21(+(_1d.major[0].value/2).toPrecision(1));
}
_3.forEach(_1d.major,function(_23,i){
_21(+(_23.value*2).toPrecision(1));
_21(+(_23.value*5).toPrecision(1));
});
}
_1d.micro=[];
return _1d;
},getTransformerFromModel:function(_24){
var _25=Math.log(this.base),_26=Math.log(_24.bounds.from)/_25,_27=_24.bounds.scale;
return function(x){
return (Math.log(x)/_25-_26)*_27;
};
},getTransformerFromPlot:function(_28){
var _29=this.base,_2a=_28.bounds.from,_2b=_28.bounds.scale;
return function(x){
return Math.pow(_29,(x/_2b))*_2a;
};
}});
});
