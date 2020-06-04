//>>built
define("dojox/date/islamic/Date",["dojo/_base/lang","dojo/_base/declare","dojo/date"],function(_1,_2,dd){
var _3=_2("dojox.date.islamic.Date",null,{_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,_GREGORIAN_EPOCH:1721425.5,_ISLAMIC_EPOCH:1948439.5,constructor:function(){
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
if(_6>0&&_6<=this.getDaysInIslamicMonth(this._month,this._year)){
this._date=_6;
}else{
var _7;
if(_6>0){
for(_7=this.getDaysInIslamicMonth(this._month,this._year);_6>_7;_6-=_7,_7=this.getDaysInIslamicMonth(this._month,this._year)){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
}
this._date=_6;
}else{
for(_7=this.getDaysInIslamicMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1);_6<=0;_7=this.getDaysInIslamicMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1)){
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
var _c=this.getDaysInIslamicMonth(this._month,this._year);
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
var _14=this._month;
var _15=this._date;
var _16=_15+Math.ceil(29.5*_14)+(_13-1)*354+Math.floor((3+(11*_13))/30)+this._ISLAMIC_EPOCH-1;
var wjd=Math.floor(_16-0.5)+0.5,_17=wjd-this._GREGORIAN_EPOCH,_18=Math.floor(_17/146097),dqc=this._mod(_17,146097),_19=Math.floor(dqc/36524),_1a=this._mod(dqc,36524),_1b=Math.floor(_1a/1461),_1c=this._mod(_1a,1461),_1d=Math.floor(_1c/365),_1e=(_18*400)+(_19*100)+(_1b*4)+_1d;
if(!(_19==4||_1d==4)){
_1e++;
}
var _1f=this._GREGORIAN_EPOCH+(365*(_1e-1))+Math.floor((_1e-1)/4)-(Math.floor((_1e-1)/100))+Math.floor((_1e-1)/400);
var _20=wjd-_1f;
var tjd=(this._GREGORIAN_EPOCH-1)+(365*(_1e-1))+Math.floor((_1e-1)/4)-(Math.floor((_1e-1)/100))+Math.floor((_1e-1)/400)+Math.floor((739/12)+((dd.isLeapYear(new Date(_1e,3,1))?-1:-2))+1);
var _21=((wjd<tjd)?0:(dd.isLeapYear(new Date(_1e,3,1))?1:2));
var _22=Math.floor((((_20+_21)*12)+373)/367);
var _23=(this._GREGORIAN_EPOCH-1)+(365*(_1e-1))+Math.floor((_1e-1)/4)-(Math.floor((_1e-1)/100))+Math.floor((_1e-1)/400)+Math.floor((((367*_22)-362)/12)+((_22<=2)?0:(dd.isLeapYear(new Date(_1e,(_22-1),1))?-1:-2))+1);
var day=(wjd-_23)+1;
var _24=new Date(_1e,(_22-1),day,this._hours,this._minutes,this._seconds,this._milliseconds);
return _24;
},fromGregorian:function(_25){
var _26=new Date(_25);
var _27=_26.getFullYear(),_28=_26.getMonth(),_29=_26.getDate();
var _2a=(this._GREGORIAN_EPOCH-1)+(365*(_27-1))+Math.floor((_27-1)/4)+(-Math.floor((_27-1)/100))+Math.floor((_27-1)/400)+Math.floor((((367*(_28+1))-362)/12)+(((_28+1)<=2)?0:(dd.isLeapYear(_26)?-1:-2))+_29);
_2a=Math.floor(_2a)+0.5;
var _2b=_2a-this._ISLAMIC_EPOCH;
var _2c=Math.floor((30*_2b+10646)/10631);
var _2d=Math.ceil((_2b-29-this._yearStart(_2c))/29.5);
_2d=Math.min(_2d,11);
var _2e=Math.ceil(_2b-this._monthStart(_2c,_2d))+1;
this._date=_2e;
this._month=_2d;
this._year=_2c;
this._hours=_26.getHours();
this._minutes=_26.getMinutes();
this._seconds=_26.getSeconds();
this._milliseconds=_26.getMilliseconds();
this._day=_26.getDay();
return this;
},valueOf:function(){
return this.toGregorian().valueOf();
},_yearStart:function(_2f){
return (_2f-1)*354+Math.floor((3+11*_2f)/30);
},_monthStart:function(_30,_31){
return Math.ceil(29.5*_31)+(_30-1)*354+Math.floor((3+11*_30)/30);
},_civilLeapYear:function(_32){
return (14+11*_32)%30<11;
},getDaysInIslamicMonth:function(_33,_34){
var _35=0;
_35=29+((_33+1)%2);
if(_33==11&&this._civilLeapYear(_34)){
_35++;
}
return _35;
},_mod:function(a,b){
return a-(b*Math.floor(a/b));
}});
_3.getDaysInIslamicMonth=function(_36){
return new _3().getDaysInIslamicMonth(_36.getMonth(),_36.getFullYear());
};
return _3;
});
