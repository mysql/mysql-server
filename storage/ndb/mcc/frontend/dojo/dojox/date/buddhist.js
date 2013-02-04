//>>built
define("dojox/date/buddhist",["dojo/_base/kernel","dojo/date","./buddhist/Date"],function(_1,dd,_2){
_1.getObject("date.buddhist",true,dojox);
_1.experimental("dojox.date.buddhist");
dojox.date.buddhist.getDaysInMonth=function(_3){
return dd.getDaysInMonth(_3.toGregorian());
};
dojox.date.buddhist.isLeapYear=function(_4){
return dd.isLeapYear(_4.toGregorian());
};
dojox.date.buddhist.compare=function(_5,_6,_7){
return dd.compare(_5,_6,_7);
};
dojox.date.buddhist.add=function(_8,_9,_a){
var _b=new _2(_8);
switch(_9){
case "day":
_b.setDate(_8.getDate(true)+_a);
break;
case "weekday":
var _c,_d;
var _e=_a%5;
if(!_e){
_c=(_a>0)?5:-5;
_d=(_a>0)?((_a-5)/5):((_a+5)/5);
}else{
_c=_e;
_d=parseInt(_a/5);
}
var _f=_8.getDay();
var adj=0;
if(_f==6&&_a>0){
adj=1;
}else{
if(_f==0&&_a<0){
adj=-1;
}
}
var _10=_f+_c;
if(_10==0||_10==6){
adj=(_a>0)?2:-2;
}
_a=(7*_d)+_c+adj;
_b.setDate(_8.getDate(true)+_a);
break;
case "year":
_b.setFullYear(_8.getFullYear()+_a);
break;
case "week":
_a*=7;
_b.setDate(_8.getDate(true)+_a);
break;
case "month":
_b.setMonth(_8.getMonth()+_a);
break;
case "hour":
_b.setHours(_8.getHours()+_a);
break;
case "minute":
_b._addMinutes(_a);
break;
case "second":
_b._addSeconds(_a);
break;
case "millisecond":
_b._addMilliseconds(_a);
break;
}
return _b;
};
dojox.date.buddhist.difference=function(_11,_12,_13){
_12=_12||new _2();
_13=_13||"day";
var _14=_12.getFullYear()-_11.getFullYear();
var _15=1;
switch(_13){
case "weekday":
var _16=Math.round(dojox.date.buddhist.difference(_11,_12,"day"));
var _17=parseInt(dojox.date.buddhist.difference(_11,_12,"week"));
var mod=_16%7;
if(mod==0){
_16=_17*5;
}else{
var adj=0;
var _18=_11.getDay();
var _19=_12.getDay();
_17=parseInt(_16/7);
mod=_16%7;
var _1a=new _2(_12);
_1a.setDate(_1a.getDate(true)+(_17*7));
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
_15=parseInt(dojox.date.buddhist.difference(_11,_12,"day")/7);
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
return dojox.date.buddhist;
});
