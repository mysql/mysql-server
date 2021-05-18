//>>built
define("dojox/date/persian/Date",["dojo/_base/lang","dojo/_base/declare","dojo/date"],function(_1,_2,dd){
var _3=_2("dojox.date.persian.Date",null,{_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,_GREGORIAN_EPOCH:1721425.5,_PERSIAN_EPOCH:1948320.5,daysInMonth:[31,31,31,31,31,31,30,30,30,30,30,29],constructor:function(){
var _4=arguments.length;
if(!_4){
this.fromGregorian(new Date());
}else{
if(_4==1){
var _5=arguments[0];
if(typeof _5=="number"){
_5=new Date(_5);
}
if(_5 instanceof Date){
this.fromGregorian(_5);
}else{
if(_5==""){
this._date=new Date("");
}else{
this._year=_5._year;
this._month=_5._month;
this._date=_5._date;
this._hours=_5._hours;
this._minutes=_5._minutes;
this._seconds=_5._seconds;
this._milliseconds=_5._milliseconds;
}
}
}else{
if(_4>=3){
this._year+=arguments[0];
this._month+=arguments[1];
this._date+=arguments[2];
this._hours+=arguments[3]||0;
this._minutes+=arguments[4]||0;
this._seconds+=arguments[5]||0;
this._milliseconds+=arguments[6]||0;
}
}
}
},getDate:function(){
return this._date;
},getMonth:function(){
return this._month;
},getFullYear:function(){
return this._year;
},getDay:function(){
return this.toGregorian().getDay();
},getHours:function(){
return this._hours;
},getMinutes:function(){
return this._minutes;
},getSeconds:function(){
return this._seconds;
},getMilliseconds:function(){
return this._milliseconds;
},setDate:function(_6){
_6=parseInt(_6);
if(_6>0&&_6<=this.getDaysInPersianMonth(this._month,this._year)){
this._date=_6;
}else{
var _7;
if(_6>0){
for(_7=this.getDaysInPersianMonth(this._month,this._year);_6>_7;_6-=_7,_7=this.getDaysInPersianMonth(this._month,this._year)){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
}
this._date=_6;
}else{
for(_7=this.getDaysInPersianMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1);_6<=0;_7=this.getDaysInPersianMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1)){
this._month--;
if(this._month<0){
this._year--;
this._month+=12;
}
_6+=_7;
}
this._date=_6;
}
}
return this;
},setFullYear:function(_8){
this._year=+_8;
},setMonth:function(_9){
this._year+=Math.floor(_9/12);
if(_9>0){
this._month=Math.floor(_9%12);
}else{
this._month=Math.floor(((_9%12)+12)%12);
}
},setHours:function(){
var _a=arguments.length;
var _b=0;
if(_a>=1){
_b=parseInt(arguments[0]);
}
if(_a>=2){
this._minutes=parseInt(arguments[1]);
}
if(_a>=3){
this._seconds=parseInt(arguments[2]);
}
if(_a==4){
this._milliseconds=parseInt(arguments[3]);
}
while(_b>=24){
this._date++;
var _c=this.getDaysInPersianMonth(this._month,this._year);
if(this._date>_c){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_c;
}
_b-=24;
}
this._hours=_b;
},_addMinutes:function(_d){
_d+=this._minutes;
this.setMinutes(_d);
this.setHours(this._hours+parseInt(_d/60));
return this;
},_addSeconds:function(_e){
_e+=this._seconds;
this.setSeconds(_e);
this._addMinutes(parseInt(_e/60));
return this;
},_addMilliseconds:function(_f){
_f+=this._milliseconds;
this.setMilliseconds(_f);
this._addSeconds(parseInt(_f/1000));
return this;
},setMinutes:function(_10){
this._minutes=_10%60;
return this;
},setSeconds:function(_11){
this._seconds=_11%60;
return this;
},setMilliseconds:function(_12){
this._milliseconds=_12%1000;
return this;
},toString:function(){
if(isNaN(this._date)){
return "Invalidate Date";
}else{
var x=new Date();
x.setHours(this._hours);
x.setMinutes(this._minutes);
x.setSeconds(this._seconds);
x.setMilliseconds(this._milliseconds);
return this._month+" "+this._date+" "+this._year+" "+x.toTimeString();
}
},toGregorian:function(){
var _13=this._year;
var _14,j;
j=this.persian_to_jd(this._year,this._month+1,this._date);
_14=this.jd_to_gregorian(j,this._month+1);
weekday=this.jwday(j);
var _15=new Date(_14[0],_14[1]-1,_14[2],this._hours,this._minutes,this._seconds,this._milliseconds);
return _15;
},fromGregorian:function(_16){
var _17=new Date(_16);
var _18=_17.getFullYear(),_19=_17.getMonth(),_1a=_17.getDate();
var _1b=this.calcGregorian(_18,_19,_1a);
this._date=_1b[2];
this._month=_1b[1];
this._year=_1b[0];
this._hours=_17.getHours();
this._minutes=_17.getMinutes();
this._seconds=_17.getSeconds();
this._milliseconds=_17.getMilliseconds();
this._day=_17.getDay();
return this;
},calcGregorian:function(_1c,_1d,day){
var j,_1e;
j=this.gregorian_to_jd(_1c,_1d+1,day)+(Math.floor(0+60*(0+60*0)+0.5)/86400);
perscal=this.jd_to_persian(j);
_1e=this.jwday(j);
return new Array(perscal[0],perscal[1],perscal[2],_1e);
},jd_to_persian:function(jd){
var _1f,_20,day,_21,_22,_23,_24,_25,_26,_27;
jd=Math.floor(jd)+0.5;
_21=jd-this.persian_to_jd(475,1,1);
_22=Math.floor(_21/1029983);
_23=this._mod(_21,1029983);
if(_23==1029982){
_24=2820;
}else{
_25=Math.floor(_23/366);
_26=this._mod(_23,366);
_24=Math.floor(((2134*_25)+(2816*_26)+2815)/1028522)+_25+1;
}
_1f=_24+(2820*_22)+474;
if(_1f<=0){
_1f--;
}
_27=(jd-this.persian_to_jd(_1f,1,1))+1;
_20=(_27<=186)?Math.ceil(_27/31):Math.ceil((_27-6)/30);
day=(jd-this.persian_to_jd(_1f,_20,1))+1;
return new Array(_1f,_20-1,day);
},persian_to_jd:function(_28,_29,day){
var _2a,_2b;
_2a=_28-((_28>=0)?474:473);
_2b=474+this._mod(_2a,2820);
return day+((_29<=7)?((_29-1)*31):(((_29-1)*30)+6))+Math.floor(((_2b*682)-110)/2816)+(_2b-1)*365+Math.floor(_2a/2820)*1029983+(this._PERSIAN_EPOCH-1);
},gregorian_to_jd:function(_2c,_2d,day){
return (this._GREGORIAN_EPOCH-1)+(365*(_2c-1))+Math.floor((_2c-1)/4)+(-Math.floor((_2c-1)/100))+Math.floor((_2c-1)/400)+Math.floor((((367*_2d)-362)/12)+((_2d<=2)?0:(this.leap_gregorian(_2c)?-1:-2))+day);
},jd_to_gregorian:function(jd,_2e){
var wjd,_2f,_30,dqc,_31,_32,_33,_34,_35,_36,_37,_38,_39;
wjd=Math.floor(jd-0.5)+0.5;
_2f=wjd-this._GREGORIAN_EPOCH;
_30=Math.floor(_2f/146097);
dqc=this._mod(_2f,146097);
_31=Math.floor(dqc/36524);
_32=this._mod(dqc,36524);
_33=Math.floor(_32/1461);
_34=this._mod(_32,1461);
_35=Math.floor(_34/365);
_37=(_30*400)+(_31*100)+(_33*4)+_35;
if(!((_31==4)||(_35==4))){
_37++;
}
_38=wjd-this.gregorian_to_jd(_37,1,1);
_39=((wjd<this.gregorian_to_jd(_37,3,1))?0:(this.leap_gregorian(_37)?1:2));
month=Math.floor((((_38+_39)*12)+373)/367);
day=(wjd-this.gregorian_to_jd(_37,month,1))+1;
return new Array(_37,month,day);
},valueOf:function(){
return this.toGregorian().valueOf();
},jwday:function(j){
return this._mod(Math.floor((j+1.5)),7);
},_yearStart:function(_3a){
return (_3a-1)*354+Math.floor((3+11*_3a)/30);
},_monthStart:function(_3b,_3c){
return Math.ceil(29.5*_3c)+(_3b-1)*354+Math.floor((3+11*_3b)/30);
},leap_gregorian:function(_3d){
return ((_3d%4)==0)&&(!(((_3d%100)==0)&&((_3d%400)!=0)));
},isLeapYear:function(j_y,j_m,j_d){
return !(j_y<0||j_y>32767||j_m<1||j_m>12||j_d<1||j_d>(this.daysInMonth[j_m-1]+(j_m==12&&!((j_y-979)%33%4))));
},getDaysInPersianMonth:function(_3e,_3f){
var _40=this.daysInMonth[_3e];
if(_3e==11&&this.isLeapYear(_3f,_3e+1,30)){
_40++;
}
return _40;
},_mod:function(a,b){
return a-(b*Math.floor(a/b));
}});
_3.getDaysInPersianMonth=function(_41){
return new _3().getDaysInPersianMonth(_41.getMonth(),_41.getFullYear());
};
return _3;
});
