//>>built
define("dojox/date/hebrew",["..","dojo/_base/lang","dojo/date","./hebrew/Date"],function(_1,_2,dd,_3){
var _4=_2.getObject("date.hebrew",true,_1);
_4.getDaysInMonth=function(_5){
return _5.getDaysInHebrewMonth(_5.getMonth(),_5.getFullYear());
};
_4.compare=function(_6,_7,_8){
if(_6 instanceof _3){
_6=_6.toGregorian();
}
if(_7 instanceof _3){
_7=_7.toGregorian();
}
return dd.compare.apply(null,arguments);
};
_4.add=function(_9,_a,_b){
var _c=new _3(_9);
switch(_a){
case "day":
_c.setDate(_9.getDate()+_b);
break;
case "weekday":
var _d=_9.getDay();
var _e=0;
if(_b<0&&_d==6){
_d=5;
_e=-1;
}
if((_d+_b)<5&&(_d+_b)>=0){
_c.setDate(_9.getDate()+_b+_e);
}else{
var _f=(_b>0)?5:-1;
var _10=(_b>0)?2:-2;
if(_b>0&&(_d==5||_d==6)){
_e=4-_d;
_d=4;
}
var _11=_d+_b-_f;
var _12=parseInt(_11/5);
var _13=_11%5;
_c.setDate(_9.getDate()-_d+_10+_12*7+_e+_13+_f);
}
break;
case "year":
_c.setFullYear(_9.getFullYear()+_b);
break;
case "week":
_b*=7;
_c.setDate(_9.getDate()+_b);
break;
case "month":
var _14=_9.getMonth(),_15=_14+_b;
if(!_9.isLeapYear(_9.getFullYear())){
if(_14<5&&_15>=5){
_15++;
}else{
if(_14>5&&_15<=5){
_15--;
}
}
}
_c.setMonth(_15);
break;
case "hour":
_c.setHours(_9.getHours()+_b);
break;
case "minute":
_c._addMinutes(_b);
break;
case "second":
_c._addSeconds(_b);
break;
case "millisecond":
_c._addMilliseconds(_b);
break;
}
return _c;
};
_4.difference=function(_16,_17,_18){
_17=_17||new _3();
_18=_18||"day";
var _19=_17.getFullYear()-_16.getFullYear();
var _1a=1;
switch(_18){
case "weekday":
var _1b=Math.round(_4.difference(_16,_17,"day"));
var _1c=parseInt(_4.difference(_16,_17,"week"));
var mod=_1b%7;
if(mod==0){
_1b=_1c*5;
}else{
var adj=0;
var _1d=_16.getDay();
var _1e=_17.getDay();
_1c=parseInt(_1b/7);
mod=_1b%7;
var _1f=new _3(_16);
_1f.setDate(_1f.getDate()+(_1c*7));
var _20=_1f.getDay();
if(_1b>0){
switch(true){
case _1d==5:
adj=-1;
break;
case _1d==6:
adj=0;
break;
case _1e==5:
adj=-1;
break;
case _1e==6:
adj=-2;
break;
case (_20+mod)>5:
adj=-2;
}
}else{
if(_1b<0){
switch(true){
case _1d==5:
adj=0;
break;
case _1d==6:
adj=1;
break;
case _1e==5:
adj=2;
break;
case _1e==6:
adj=1;
break;
case (_20+mod)<0:
adj=2;
}
}
}
_1b+=adj;
_1b-=(_1c*2);
}
_1a=_1b;
break;
case "year":
_1a=_19;
break;
case "month":
var _21=(_17.toGregorian()>_16.toGregorian())?_17:_16;
var _22=(_17.toGregorian()>_16.toGregorian())?_16:_17;
var _23=_21.getMonth();
var _24=_22.getMonth();
if(_19==0){
_1a=(!_17.isLeapYear(_17.getFullYear())&&_21.getMonth()>5&&_22.getMonth()<=5)?(_21.getMonth()-_22.getMonth()-1):(_21.getMonth()-_22.getMonth());
}else{
_1a=(!_22.isLeapYear(_22.getFullYear())&&_24<6)?(13-_24-1):(13-_24);
_1a+=(!_21.isLeapYear(_21.getFullYear())&&_23>5)?(_23-1):_23;
var i=_22.getFullYear()+1;
var e=_21.getFullYear();
for(i;i<e;i++){
_1a+=_22.isLeapYear(i)?13:12;
}
}
if(_17.toGregorian()<_16.toGregorian()){
_1a=-_1a;
}
break;
case "week":
_1a=parseInt(_4.difference(_16,_17,"day")/7);
break;
case "day":
_1a/=24;
case "hour":
_1a/=60;
case "minute":
_1a/=60;
case "second":
_1a/=1000;
case "millisecond":
_1a*=_17.toGregorian().getTime()-_16.toGregorian().getTime();
}
return Math.round(_1a);
};
return _4;
});
