//>>built
define("dojox/date/hebrew",["dojo/_base/kernel","dojo/date","./hebrew/Date"],function(_1,dd,_2){
_1.getObject("date.hebrew",true,dojox);
_1.experimental("dojox.date.hebrew");
dojox.date.hebrew.getDaysInMonth=function(_3){
return _3.getDaysInHebrewMonth(_3.getMonth(),_3.getFullYear());
};
dojox.date.hebrew.compare=function(_4,_5,_6){
if(_4 instanceof _2){
_4=_4.toGregorian();
}
if(_5 instanceof _2){
_5=_5.toGregorian();
}
return dd.compare.apply(null,arguments);
};
dojox.date.hebrew.add=function(_7,_8,_9){
var _a=new _2(_7);
switch(_8){
case "day":
_a.setDate(_7.getDate()+_9);
break;
case "weekday":
var _b=_7.getDay();
var _c=0;
if(_9<0&&_b==6){
_b=5;
_c=-1;
}
if((_b+_9)<5&&(_b+_9)>=0){
_a.setDate(_7.getDate()+_9+_c);
}else{
var _d=(_9>0)?5:-1;
var _e=(_9>0)?2:-2;
if(_9>0&&(_b==5||_b==6)){
_c=4-_b;
_b=4;
}
var _f=_b+_9-_d;
var _10=parseInt(_f/5);
var _11=_f%5;
_a.setDate(_7.getDate()-_b+_e+_10*7+_c+_11+_d);
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
var _12=_7.getMonth(),_13=_12+_9;
if(!_7.isLeapYear(_7.getFullYear())){
if(_12<5&&_13>=5){
_13++;
}else{
if(_12>5&&_13<=5){
_13--;
}
}
}
_a.setMonth(_13);
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
dojox.date.hebrew.difference=function(_14,_15,_16){
_15=_15||new _2();
_16=_16||"day";
var _17=_15.getFullYear()-_14.getFullYear();
var _18=1;
switch(_16){
case "weekday":
var _19=Math.round(dojox.date.hebrew.difference(_14,_15,"day"));
var _1a=parseInt(dojox.date.hebrew.difference(_14,_15,"week"));
var mod=_19%7;
if(mod==0){
_19=_1a*5;
}else{
var adj=0;
var _1b=_14.getDay();
var _1c=_15.getDay();
_1a=parseInt(_19/7);
mod=_19%7;
var _1d=new _2(_14);
_1d.setDate(_1d.getDate()+(_1a*7));
var _1e=_1d.getDay();
if(_19>0){
switch(true){
case _1b==5:
adj=-1;
break;
case _1b==6:
adj=0;
break;
case _1c==5:
adj=-1;
break;
case _1c==6:
adj=-2;
break;
case (_1e+mod)>5:
adj=-2;
}
}else{
if(_19<0){
switch(true){
case _1b==5:
adj=0;
break;
case _1b==6:
adj=1;
break;
case _1c==5:
adj=2;
break;
case _1c==6:
adj=1;
break;
case (_1e+mod)<0:
adj=2;
}
}
}
_19+=adj;
_19-=(_1a*2);
}
_18=_19;
break;
case "year":
_18=_17;
break;
case "month":
var _1f=(_15.toGregorian()>_14.toGregorian())?_15:_14;
var _20=(_15.toGregorian()>_14.toGregorian())?_14:_15;
var _21=_1f.getMonth();
var _22=_20.getMonth();
if(_17==0){
_18=(!_15.isLeapYear(_15.getFullYear())&&_1f.getMonth()>5&&_20.getMonth()<=5)?(_1f.getMonth()-_20.getMonth()-1):(_1f.getMonth()-_20.getMonth());
}else{
_18=(!_20.isLeapYear(_20.getFullYear())&&_22<6)?(13-_22-1):(13-_22);
_18+=(!_1f.isLeapYear(_1f.getFullYear())&&_21>5)?(_21-1):_21;
var i=_20.getFullYear()+1;
var e=_1f.getFullYear();
for(i;i<e;i++){
_18+=_20.isLeapYear(i)?13:12;
}
}
if(_15.toGregorian()<_14.toGregorian()){
_18=-_18;
}
break;
case "week":
_18=parseInt(dojox.date.hebrew.difference(_14,_15,"day")/7);
break;
case "day":
_18/=24;
case "hour":
_18/=60;
case "minute":
_18/=60;
case "second":
_18/=1000;
case "millisecond":
_18*=_15.toGregorian().getTime()-_14.toGregorian().getTime();
}
return Math.round(_18);
};
return dojox.date.hebrew;
});
