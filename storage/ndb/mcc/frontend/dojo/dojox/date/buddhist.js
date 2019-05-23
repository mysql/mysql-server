//>>built
define("dojox/date/buddhist",["..","dojo/_base/lang","dojo/date","./buddhist/Date"],function(_1,_2,dd,_3){
var _4=_2.getObject("date.buddhist",true,_1);
_4.getDaysInMonth=function(_5){
return dd.getDaysInMonth(_5.toGregorian());
};
_4.isLeapYear=function(_6){
return dd.isLeapYear(_6.toGregorian());
};
_4.compare=function(_7,_8,_9){
return dd.compare(_7,_8,_9);
};
_4.add=function(_a,_b,_c){
var _d=new _3(_a);
switch(_b){
case "day":
_d.setDate(_a.getDate(true)+_c);
break;
case "weekday":
var _e,_f;
var mod=_c%5;
if(!mod){
_e=(_c>0)?5:-5;
_f=(_c>0)?((_c-5)/5):((_c+5)/5);
}else{
_e=mod;
_f=parseInt(_c/5);
}
var _10=_a.getDay();
var adj=0;
if(_10==6&&_c>0){
adj=1;
}else{
if(_10==0&&_c<0){
adj=-1;
}
}
var _11=_10+_e;
if(_11==0||_11==6){
adj=(_c>0)?2:-2;
}
_c=(7*_f)+_e+adj;
_d.setDate(_a.getDate(true)+_c);
break;
case "year":
_d.setFullYear(_a.getFullYear()+_c);
break;
case "week":
_c*=7;
_d.setDate(_a.getDate(true)+_c);
break;
case "month":
_d.setMonth(_a.getMonth()+_c);
break;
case "hour":
_d.setHours(_a.getHours()+_c);
break;
case "minute":
_d._addMinutes(_c);
break;
case "second":
_d._addSeconds(_c);
break;
case "millisecond":
_d._addMilliseconds(_c);
break;
}
return _d;
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
var _1b=new _3(_13);
_1b.setDate(_1b.getDate(true)+(_18*7));
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
