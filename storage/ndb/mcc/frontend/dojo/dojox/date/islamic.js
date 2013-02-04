//>>built
define("dojox/date/islamic",["dojo/_base/kernel","dojo/date","./islamic/Date"],function(_1,dd,_2){
_1.getObject("date.islamic",true,dojox);
_1.experimental("dojox.date.islamic");
dojox.date.islamic.getDaysInMonth=function(_3){
return _3.getDaysInIslamicMonth(_3.getMonth(),_3.getFullYear());
};
dojox.date.islamic.compare=function(_4,_5,_6){
if(_4 instanceof _2){
_4=_4.toGregorian();
}
if(_5 instanceof _2){
_5=_5.toGregorian();
}
return dd.compare.apply(null,arguments);
};
dojox.date.islamic.add=function(_7,_8,_9){
var _a=new _2(_7);
switch(_8){
case "day":
_a.setDate(_7.getDate()+_9);
break;
case "weekday":
var _b=_7.getDay();
if(((_b+_9)<5)&&((_b+_9)>0)){
_a.setDate(_7.getDate()+_9);
}else{
var _c=0,_d=0;
if(_b==5){
_b=4;
_d=(_9>0)?-1:1;
}else{
if(_b==6){
_b=4;
_d=(_9>0)?-2:2;
}
}
var _e=(_9>0)?(5-_b-1):-_b;
var _f=_9-_e;
var div=parseInt(_f/5);
if(_f%5!=0){
_c=(_9>0)?2:-2;
}
_c=_c+div*7+_f%5+_e;
_a.setDate(_7.getDate()+_c+_d);
}
break;
case "year":
_a.setFullYear(_7.getFullYear()+_9);
break;
case "week":
_9*=7;
_a.setDate(_7.getDate()+_9);
break;
case "month":
var _10=_7.getMonth();
_a.setMonth(_10+_9);
break;
case "hour":
_a.setHours(_7.getHours()+_9);
break;
case "minute":
_a._addMinutes(_9);
break;
case "second":
_a._addSeconds(_9);
break;
case "millisecond":
_a._addMilliseconds(_9);
break;
}
return _a;
};
dojox.date.islamic.difference=function(_11,_12,_13){
_12=_12||new _2();
_13=_13||"day";
var _14=_12.getFullYear()-_11.getFullYear();
var _15=1;
switch(_13){
case "weekday":
var _16=Math.round(dojox.date.islamic.difference(_11,_12,"day"));
var _17=parseInt(dojox.date.islamic.difference(_11,_12,"week"));
var mod=_16%7;
if(mod==0){
_16=_17*5;
}else{
var adj=0;
var _18=_11.getDay();
var _19=_12.getDay();
_17=parseInt(_16/7);
mod=_16%7;
var _1a=new _2(_11);
_1a.setDate(_1a.getDate()+(_17*7));
var _1b=_1a.getDay();
if(_16>0){
switch(true){
case _18==5:
adj=-1;
break;
case _18==6:
adj=0;
break;
case _19==5:
adj=-1;
break;
case _19==6:
adj=-2;
break;
case (_1b+mod)>5:
adj=-2;
}
}else{
if(_16<0){
switch(true){
case _18==5:
adj=0;
break;
case _18==6:
adj=1;
break;
case _19==5:
adj=2;
break;
case _19==6:
adj=1;
break;
case (_1b+mod)<0:
adj=2;
}
}
}
_16+=adj;
_16-=(_17*2);
}
_15=_16;
break;
case "year":
_15=_14;
break;
case "month":
var _1c=(_12.toGregorian()>_11.toGregorian())?_12:_11;
var _1d=(_12.toGregorian()>_11.toGregorian())?_11:_12;
var _1e=_1c.getMonth();
var _1f=_1d.getMonth();
if(_14==0){
_15=_1c.getMonth()-_1d.getMonth();
}else{
_15=12-_1f;
_15+=_1e;
var i=_1d.getFullYear()+1;
var e=_1c.getFullYear();
for(i;i<e;i++){
_15+=12;
}
}
if(_12.toGregorian()<_11.toGregorian()){
_15=-_15;
}
break;
case "week":
_15=parseInt(dojox.date.islamic.difference(_11,_12,"day")/7);
break;
case "day":
_15/=24;
case "hour":
_15/=60;
case "minute":
_15/=60;
case "second":
_15/=1000;
case "millisecond":
_15*=_12.toGregorian().getTime()-_11.toGregorian().getTime();
}
return Math.round(_15);
};
return dojox.date.islamic;
});
