//>>built
define("dojox/date/buddhist/Date",["dojo/_base/kernel","dojo/_base/declare","dojo/date"],function(_1,_2,dd){
_1.getObject("date.buddhist.Date",true,dojox);
_1.experimental("dojox.date.buddhist.Date");
_1.declare("dojox.date.buddhist.Date",null,{_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,constructor:function(){
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
},getDate:function(_5){
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
},setDate:function(_6){
_6=parseInt(_6);
if(_6>0&&_6<=this._getDaysInMonth(this._month,this._year)){
this._date=_6;
}else{
var _7;
if(_6>0){
for(_7=this._getDaysInMonth(this._month,this._year);_6>_7;_6-=_7,_7=this._getDaysInMonth(this._month,this._year)){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
}
this._date=_6;
}else{
for(_7=this._getDaysInMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1);_6<=0;_7=this._getDaysInMonth((this._month-1)>=0?(this._month-1):11,((this._month-1)>=0)?this._year:this._year-1)){
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
},setFullYear:function(_8,_9,_a){
this._year=parseInt(_8);
},setMonth:function(_b){
this._year+=Math.floor(_b/12);
this._month=Math.floor(_b%12);
for(;this._month<0;this._month=this._month+12){
}
},setHours:function(){
var _c=arguments.length;
var _d=0;
if(_c>=1){
_d=parseInt(arguments[0]);
}
if(_c>=2){
this._minutes=parseInt(arguments[1]);
}
if(_c>=3){
this._seconds=parseInt(arguments[2]);
}
if(_c==4){
this._milliseconds=parseInt(arguments[3]);
}
while(_d>=24){
this._date++;
var _e=this._getDaysInMonth(this._month,this._year);
if(this._date>_e){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_e;
}
_d-=24;
}
this._hours=_d;
},_addMinutes:function(_f){
_f+=this._minutes;
this.setMinutes(_f);
this.setHours(this._hours+parseInt(_f/60));
return this;
},_addSeconds:function(_10){
_10+=this._seconds;
this.setSeconds(_10);
this._addMinutes(parseInt(_10/60));
return this;
},_addMilliseconds:function(_11){
_11+=this._milliseconds;
this.setMilliseconds(_11);
this._addSeconds(parseInt(_11/1000));
return this;
},setMinutes:function(_12){
this._minutes=_12%60;
return this;
},setSeconds:function(_13){
this._seconds=_13%60;
return this;
},setMilliseconds:function(_14){
this._milliseconds=_14%1000;
return this;
},toString:function(){
return this._date+", "+this._month+", "+this._year+"  "+this._hours+":"+this._minutes+":"+this._seconds;
},_getDaysInMonth:function(_15,_16){
return dd.getDaysInMonth(new Date(_16-543,_15));
},fromGregorian:function(_17){
var _18=new Date(_17);
this._date=_18.getDate();
this._month=_18.getMonth();
this._year=_18.getFullYear()+543;
this._hours=_18.getHours();
this._minutes=_18.getMinutes();
this._seconds=_18.getSeconds();
this._milliseconds=_18.getMilliseconds();
this._day=_18.getDay();
return this;
},toGregorian:function(){
return new Date(this._year-543,this._month,this._date,this._hours,this._minutes,this._seconds,this._milliseconds);
},getDay:function(){
return this.toGregorian().getDay();
}});
dojox.date.buddhist.Date.prototype.valueOf=function(){
return this.toGregorian().valueOf();
};
return dojox.date.buddhist.Date;
});
