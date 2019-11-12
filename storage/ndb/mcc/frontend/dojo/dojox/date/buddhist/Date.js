//>>built
define("dojox/date/buddhist/Date",["dojo/_base/lang","dojo/_base/declare","dojo/date"],function(_1,_2,dd){
var _3=_2("dojox.date.buddhist.Date",null,{_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,constructor:function(){
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
if(this._month>11){
console.warn("the month is incorrect , set 0");
this._month=0;
}
this._hours+=arguments[3]||0;
this._minutes+=arguments[4]||0;
this._seconds+=arguments[5]||0;
this._milliseconds+=arguments[6]||0;
}
}
}
},getDate:function(_6){
return parseInt(this._date);
},getMonth:function(){
return parseInt(this._month);
},getFullYear:function(){
return parseInt(this._year);
},getHours:function(){
return this._hours;
},getMinutes:function(){
return this._minutes;
},getSeconds:function(){
return this._seconds;
},getMilliseconds:function(){
return this._milliseconds;
},setDate:function(_7){
_7=parseInt(_7);
if(_7>0&&_7<=this._getDaysInMonth(this._month,this._year)){
this._date=_7;
}else{
var _8;
if(_7>0){
for(_8=this._getDaysInMonth(this._month,this._year);_7>_8;_7-=_8,_8=this._getDaysInMonth(this._month,this._year)){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
}
this._date=_7;
}else{
for(_8=this._getDaysInMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1);_7<=0;_8=this._getDaysInMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1)){
this._month--;
if(this._month<0){
this._year--;
this._month+=12;
}
_7+=_8;
}
this._date=_7;
}
}
return this;
},setFullYear:function(_9,_a,_b){
this._year=parseInt(_9);
},setMonth:function(_c){
this._year+=Math.floor(_c/12);
this._month=Math.floor(_c%12);
for(;this._month<0;this._month=this._month+12){
}
},setHours:function(){
var _d=arguments.length;
var _e=0;
if(_d>=1){
_e=parseInt(arguments[0]);
}
if(_d>=2){
this._minutes=parseInt(arguments[1]);
}
if(_d>=3){
this._seconds=parseInt(arguments[2]);
}
if(_d==4){
this._milliseconds=parseInt(arguments[3]);
}
while(_e>=24){
this._date++;
var _f=this._getDaysInMonth(this._month,this._year);
if(this._date>_f){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_f;
}
_e-=24;
}
this._hours=_e;
},_addMinutes:function(_10){
_10+=this._minutes;
this.setMinutes(_10);
this.setHours(this._hours+parseInt(_10/60));
return this;
},_addSeconds:function(_11){
_11+=this._seconds;
this.setSeconds(_11);
this._addMinutes(parseInt(_11/60));
return this;
},_addMilliseconds:function(_12){
_12+=this._milliseconds;
this.setMilliseconds(_12);
this._addSeconds(parseInt(_12/1000));
return this;
},setMinutes:function(_13){
this._minutes=_13%60;
return this;
},setSeconds:function(_14){
this._seconds=_14%60;
return this;
},setMilliseconds:function(_15){
this._milliseconds=_15%1000;
return this;
},toString:function(){
return isNaN(this._date)?"Invalid Date":this._date+", "+this._month+", "+this._year+"  "+this._hours+":"+this._minutes+":"+this._seconds;
},_getDaysInMonth:function(_16,_17){
return dd.getDaysInMonth(new Date(_17-543,_16));
},fromGregorian:function(_18){
var _19=new Date(_18);
this._date=_19.getDate();
this._month=_19.getMonth();
this._year=_19.getFullYear()+543;
this._hours=_19.getHours();
this._minutes=_19.getMinutes();
this._seconds=_19.getSeconds();
this._milliseconds=_19.getMilliseconds();
this._day=_19.getDay();
return this;
},toGregorian:function(){
return new Date(this._year-543,this._month,this._date,this._hours,this._minutes,this._seconds,this._milliseconds);
},getDay:function(){
return this.toGregorian().getDay();
}});
_3.prototype.valueOf=function(){
return this.toGregorian().valueOf();
};
return _3;
});
