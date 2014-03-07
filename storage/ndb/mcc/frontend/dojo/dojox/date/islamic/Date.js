//>>built
define("dojox/date/islamic/Date",["dojo/_base/kernel","dojo/_base/declare","dojo/date"],function(_1,_2,dd){
_1.getObject("date.buddhist.Date",true,dojox);
_1.experimental("dojox.date.buddhist.Date");
_1.requireLocalization("dojo.cldr","islamic");
_1.declare("dojox.date.islamic.Date",null,{_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,_GREGORIAN_EPOCH:1721425.5,_ISLAMIC_EPOCH:1948439.5,constructor:function(){
var _3=arguments.length;
if(!_3){
this.fromGregorian(new Date());
}else{
if(_3==1){
var _4=arguments[0];
if(typeof _4=="number"){
_4=new Date(_4);
}
if(_4 instanceof Date){
this.fromGregorian(_4);
}else{
if(_4==""){
this._date=new Date("");
}else{
this._year=_4._year;
this._month=_4._month;
this._date=_4._date;
this._hours=_4._hours;
this._minutes=_4._minutes;
this._seconds=_4._seconds;
this._milliseconds=_4._milliseconds;
}
}
}else{
if(_3>=3){
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
},setDate:function(_5){
_5=parseInt(_5);
if(_5>0&&_5<=this.getDaysInIslamicMonth(this._month,this._year)){
this._date=_5;
}else{
var _6;
if(_5>0){
for(_6=this.getDaysInIslamicMonth(this._month,this._year);_5>_6;_5-=_6,_6=this.getDaysInIslamicMonth(this._month,this._year)){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
}
this._date=_5;
}else{
for(_6=this.getDaysInIslamicMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1);_5<=0;_6=this.getDaysInIslamicMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1)){
this._month--;
if(this._month<0){
this._year--;
this._month+=12;
}
_5+=_6;
}
this._date=_5;
}
}
return this;
},setFullYear:function(_7){
this._year=+_7;
},setMonth:function(_8){
this._year+=Math.floor(_8/12);
if(_8>0){
this._month=Math.floor(_8%12);
}else{
this._month=Math.floor(((_8%12)+12)%12);
}
},setHours:function(){
var _9=arguments.length;
var _a=0;
if(_9>=1){
_a=parseInt(arguments[0]);
}
if(_9>=2){
this._minutes=parseInt(arguments[1]);
}
if(_9>=3){
this._seconds=parseInt(arguments[2]);
}
if(_9==4){
this._milliseconds=parseInt(arguments[3]);
}
while(_a>=24){
this._date++;
var _b=this.getDaysInIslamicMonth(this._month,this._year);
if(this._date>_b){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_b;
}
_a-=24;
}
this._hours=_a;
},_addMinutes:function(_c){
_c+=this._minutes;
this.setMinutes(_c);
this.setHours(this._hours+parseInt(_c/60));
return this;
},_addSeconds:function(_d){
_d+=this._seconds;
this.setSeconds(_d);
this._addMinutes(parseInt(_d/60));
return this;
},_addMilliseconds:function(_e){
_e+=this._milliseconds;
this.setMilliseconds(_e);
this._addSeconds(parseInt(_e/1000));
return this;
},setMinutes:function(_f){
this._minutes=_f%60;
return this;
},setSeconds:function(_10){
this._seconds=_10%60;
return this;
},setMilliseconds:function(_11){
this._milliseconds=_11%1000;
return this;
},toString:function(){
var x=new Date();
x.setHours(this._hours);
x.setMinutes(this._minutes);
x.setSeconds(this._seconds);
x.setMilliseconds(this._milliseconds);
return this._month+" "+this._date+" "+this._year+" "+x.toTimeString();
},toGregorian:function(){
var _12=this._year;
var _13=this._month;
var _14=this._date;
var _15=_14+Math.ceil(29.5*_13)+(_12-1)*354+Math.floor((3+(11*_12))/30)+this._ISLAMIC_EPOCH-1;
var wjd=Math.floor(_15-0.5)+0.5,_16=wjd-this._GREGORIAN_EPOCH,_17=Math.floor(_16/146097),dqc=this._mod(_16,146097),_18=Math.floor(dqc/36524),_19=this._mod(dqc,36524),_1a=Math.floor(_19/1461),_1b=this._mod(_19,1461),_1c=Math.floor(_1b/365),_1d=(_17*400)+(_18*100)+(_1a*4)+_1c;
if(!(_18==4||_1c==4)){
_1d++;
}
var _1e=this._GREGORIAN_EPOCH+(365*(_1d-1))+Math.floor((_1d-1)/4)-(Math.floor((_1d-1)/100))+Math.floor((_1d-1)/400);
var _1f=wjd-_1e;
var tjd=(this._GREGORIAN_EPOCH-1)+(365*(_1d-1))+Math.floor((_1d-1)/4)-(Math.floor((_1d-1)/100))+Math.floor((_1d-1)/400)+Math.floor((739/12)+((dd.isLeapYear(new Date(_1d,3,1))?-1:-2))+1);
var _20=((wjd<tjd)?0:(dd.isLeapYear(new Date(_1d,3,1))?1:2));
var _21=Math.floor((((_1f+_20)*12)+373)/367);
var _22=(this._GREGORIAN_EPOCH-1)+(365*(_1d-1))+Math.floor((_1d-1)/4)-(Math.floor((_1d-1)/100))+Math.floor((_1d-1)/400)+Math.floor((((367*_21)-362)/12)+((_21<=2)?0:(dd.isLeapYear(new Date(_1d,_21,1))?-1:-2))+1);
var day=(wjd-_22)+1;
var _23=new Date(_1d,(_21-1),day,this._hours,this._minutes,this._seconds,this._milliseconds);
return _23;
},fromGregorian:function(_24){
var _25=new Date(_24);
var _26=_25.getFullYear(),_27=_25.getMonth(),_28=_25.getDate();
var _29=(this._GREGORIAN_EPOCH-1)+(365*(_26-1))+Math.floor((_26-1)/4)+(-Math.floor((_26-1)/100))+Math.floor((_26-1)/400)+Math.floor((((367*(_27+1))-362)/12)+(((_27+1)<=2)?0:(dd.isLeapYear(_25)?-1:-2))+_28);
_29=Math.floor(_29)+0.5;
var _2a=_29-this._ISLAMIC_EPOCH;
var _2b=Math.floor((30*_2a+10646)/10631);
var _2c=Math.ceil((_2a-29-this._yearStart(_2b))/29.5);
_2c=Math.min(_2c,11);
var _2d=Math.ceil(_2a-this._monthStart(_2b,_2c))+1;
this._date=_2d;
this._month=_2c;
this._year=_2b;
this._hours=_25.getHours();
this._minutes=_25.getMinutes();
this._seconds=_25.getSeconds();
this._milliseconds=_25.getMilliseconds();
this._day=_25.getDay();
return this;
},valueOf:function(){
return this.toGregorian().valueOf();
},_yearStart:function(_2e){
return (_2e-1)*354+Math.floor((3+11*_2e)/30);
},_monthStart:function(_2f,_30){
return Math.ceil(29.5*_30)+(_2f-1)*354+Math.floor((3+11*_2f)/30);
},_civilLeapYear:function(_31){
return (14+11*_31)%30<11;
},getDaysInIslamicMonth:function(_32,_33){
var _34=0;
_34=29+((_32+1)%2);
if(_32==11&&this._civilLeapYear(_33)){
_34++;
}
return _34;
},_mod:function(a,b){
return a-(b*Math.floor(a/b));
}});
dojox.date.islamic.Date.getDaysInIslamicMonth=function(_35){
return new dojox.date.islamic.Date().getDaysInIslamicMonth(_35.getMonth(),_35.getFullYear());
};
return dojox.date.islamic.Date;
});
