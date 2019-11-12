//>>built
define("dojox/date/islamic",["..","dojo/_base/lang","dojo/date","./islamic/Date"],function(_1,_2,dd,_3){
var _4=_2.getObject("date.islamic",true,_1);
_4.getDaysInMonth=function(_5){
return _5.getDaysInIslamicMonth(_5.getMonth(),_5.getFullYear());
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
if(((_d+_b)<5)&&((_d+_b)>0)){
_c.setDate(_9.getDate()+_b);
}else{
var _e=0,_f=0;
if(_d==5){
_d=4;
_f=(_b>0)?-1:1;
}else{
if(_d==6){
_d=4;
_f=(_b>0)?-2:2;
}
}
var add=(_b>0)?(5-_d-1):-_d;
var _10=_b-add;
var div=parseInt(_10/5);
if(_10%5!=0){
_e=(_b>0)?2:-2;
}
_e=_e+div*7+_10%5+add;
_c.setDate(_9.getDate()+_e+_f);
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
var _11=_9.getMonth();
_c.setMonth(_11+_b);
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
_4.difference=function(_12,_13,_14){
_13=_13||new _3();
_14=_14||"day";
var _15=_13.getFullYear()-_12.getFullYear();
var _16=1;
switch(_14){
case "weekday":
var _17=Math.round(_4.difference(_12,_13,"day"));
var _18=parseInt(_4.difference(_12,_13,"week"));
var mod=_17%7;
if(mod==0){
_17=_18*5;
}else{
var adj=0;
var _19=_12.getDay();
var _1a=_13.getDay();
_18=parseInt(_17/7);
mod=_17%7;
var _1b=new _3(_12);
_1b.setDate(_1b.getDate()+(_18*7));
var _1c=_1b.getDay();
if(_17>0){
switch(true){
case _19==5:
adj=-1;
break;
case _19==6:
adj=0;
break;
case _1a==5:
adj=-1;
break;
case _1a==6:
adj=-2;
break;
case (_1c+mod)>5:
adj=-2;
}
}else{
if(_17<0){
switch(true){
case _19==5:
adj=0;
break;
case _19==6:
adj=1;
break;
case _1a==5:
adj=2;
break;
case _1a==6:
adj=1;
break;
case (_1c+mod)<0:
adj=2;
}
}
}
_17+=adj;
_17-=(_18*2);
}
_16=_17;
break;
case "year":
_16=_15;
break;
case "month":
var _1d=(_13.toGregorian()>_12.toGregorian())?_13:_12;
var _1e=(_13.toGregorian()>_12.toGregorian())?_12:_13;
var _1f=_1d.getMonth();
var _20=_1e.getMonth();
if(_15==0){
_16=_1d.getMonth()-_1e.getMonth();
}else{
_16=12-_20;
_16+=_1f;
var i=_1e.getFullYear()+1;
var e=_1d.getFullYear();
for(i;i<e;i++){
_16+=12;
}
}
if(_13.toGregorian()<_12.toGregorian()){
_16=-_16;
}
break;
case "week":
_16=parseInt(_4.difference(_12,_13,"day")/7);
break;
case "day":
_16/=24;
case "hour":
_16/=60;
case "minute":
_16/=60;
case "second":
_16/=1000;
case "millisecond":
_16*=_13.toGregorian().getTime()-_12.toGregorian().getTime();
}
return Math.round(_16);
};
return _4;
});
