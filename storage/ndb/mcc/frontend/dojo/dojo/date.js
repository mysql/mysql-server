/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/date",["./_base/kernel","./_base/lang"],function(_1,_2){
_2.getObject("date",true,_1);
_1.date.getDaysInMonth=function(_3){
var _4=_3.getMonth();
var _5=[31,28,31,30,31,30,31,31,30,31,30,31];
if(_4==1&&_1.date.isLeapYear(_3)){
return 29;
}
return _5[_4];
};
_1.date.isLeapYear=function(_6){
var _7=_6.getFullYear();
return !(_7%400)||(!(_7%4)&&!!(_7%100));
};
_1.date.getTimezoneName=function(_8){
var _9=_8.toString();
var tz="";
var _a;
var _b=_9.indexOf("(");
if(_b>-1){
tz=_9.substring(++_b,_9.indexOf(")"));
}else{
var _c=/([A-Z\/]+) \d{4}$/;
if((_a=_9.match(_c))){
tz=_a[1];
}else{
_9=_8.toLocaleString();
_c=/ ([A-Z\/]+)$/;
if((_a=_9.match(_c))){
tz=_a[1];
}
}
}
return (tz=="AM"||tz=="PM")?"":tz;
};
_1.date.compare=function(_d,_e,_f){
_d=new Date(+_d);
_e=new Date(+(_e||new Date()));
if(_f=="date"){
_d.setHours(0,0,0,0);
_e.setHours(0,0,0,0);
}else{
if(_f=="time"){
_d.setFullYear(0,0,0);
_e.setFullYear(0,0,0);
}
}
if(_d>_e){
return 1;
}
if(_d<_e){
return -1;
}
return 0;
};
_1.date.add=function(_10,_11,_12){
var sum=new Date(+_10);
var _13=false;
var _14="Date";
switch(_11){
case "day":
break;
case "weekday":
var _15,_16;
var mod=_12%5;
if(!mod){
_15=(_12>0)?5:-5;
_16=(_12>0)?((_12-5)/5):((_12+5)/5);
}else{
_15=mod;
_16=parseInt(_12/5);
}
var _17=_10.getDay();
var adj=0;
if(_17==6&&_12>0){
adj=1;
}else{
if(_17==0&&_12<0){
adj=-1;
}
}
var _18=_17+_15;
if(_18==0||_18==6){
adj=(_12>0)?2:-2;
}
_12=(7*_16)+_15+adj;
break;
case "year":
_14="FullYear";
_13=true;
break;
case "week":
_12*=7;
break;
case "quarter":
_12*=3;
case "month":
_13=true;
_14="Month";
break;
default:
_14="UTC"+_11.charAt(0).toUpperCase()+_11.substring(1)+"s";
}
if(_14){
sum["set"+_14](sum["get"+_14]()+_12);
}
if(_13&&(sum.getDate()<_10.getDate())){
sum.setDate(0);
}
return sum;
};
_1.date.difference=function(_19,_1a,_1b){
_1a=_1a||new Date();
_1b=_1b||"day";
var _1c=_1a.getFullYear()-_19.getFullYear();
var _1d=1;
switch(_1b){
case "quarter":
var m1=_19.getMonth();
var m2=_1a.getMonth();
var q1=Math.floor(m1/3)+1;
var q2=Math.floor(m2/3)+1;
q2+=(_1c*4);
_1d=q2-q1;
break;
case "weekday":
var _1e=Math.round(_1.date.difference(_19,_1a,"day"));
var _1f=parseInt(_1.date.difference(_19,_1a,"week"));
var mod=_1e%7;
if(mod==0){
_1e=_1f*5;
}else{
var adj=0;
var _20=_19.getDay();
var _21=_1a.getDay();
_1f=parseInt(_1e/7);
mod=_1e%7;
var _22=new Date(_19);
_22.setDate(_22.getDate()+(_1f*7));
var _23=_22.getDay();
if(_1e>0){
switch(true){
case _20==6:
adj=-1;
break;
case _20==0:
adj=0;
break;
case _21==6:
adj=-1;
break;
case _21==0:
adj=-2;
break;
case (_23+mod)>5:
adj=-2;
}
}else{
if(_1e<0){
switch(true){
case _20==6:
adj=0;
break;
case _20==0:
adj=1;
break;
case _21==6:
adj=2;
break;
case _21==0:
adj=1;
break;
case (_23+mod)<0:
adj=2;
}
}
}
_1e+=adj;
_1e-=(_1f*2);
}
_1d=_1e;
break;
case "year":
_1d=_1c;
break;
case "month":
_1d=(_1a.getMonth()-_19.getMonth())+(_1c*12);
break;
case "week":
_1d=parseInt(_1.date.difference(_19,_1a,"day")/7);
break;
case "day":
_1d/=24;
case "hour":
_1d/=60;
case "minute":
_1d/=60;
case "second":
_1d/=1000;
case "millisecond":
_1d*=_1a.getTime()-_19.getTime();
}
return Math.round(_1d);
};
return _1.date;
});
