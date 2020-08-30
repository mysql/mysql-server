//>>built
define("dojox/date/umalqura/Date",["dojo/_base/lang","dojo/_base/declare","dojo/date","../islamic/Date"],function(_1,_2,dd){
var _3=_2("dojox.date.umalqura.Date",null,{_MONTH_LENGTH:["101010101010","110101010100","111011001001","011011010100","011011101010","001101101100","101010101101","010101010101","011010101001","011110010010","101110101001","010111010100","101011011010","010101011100","110100101101","011010010101","011101001010","101101010100","101101101010","010110101101","010010101110","101001001111","010100010111","011010001011","011010100101","101011010101","001011010110","100101011011","010010011101","101001001101","110100100110","110110010101","010110101100","100110110110","001010111010","101001011011","010100101011","101010010101","011011001010","101011101001","001011110100","100101110110","001010110110","100101010110","101011001010","101110100100","101111010010","010111011001","001011011100","100101101101","010101001101","101010100101","101101010010","101110100101","010110110100","100110110110","010101010111","001010010111","010101001011","011010100011","011101010010","101101100101","010101101010","101010101011","010100101011","110010010101","110101001010","110110100101","010111001010","101011010110","100101010111","010010101011","100101001011","101010100101","101101010010","101101101010","010101110101","001001110110","100010110111","010001011011","010101010101","010110101001","010110110100","100111011010","010011011101","001001101110","100100110110","101010101010","110101010100","110110110010","010111010101","001011011010","100101011011","010010101011","101001010101","101101001001","101101100100","101101110001","010110110100","101010110101","101001010101","110100100101","111010010010","111011001001","011011010100","101011101001","100101101011","010010101011","101010010011","110101001001","110110100100","110110110010","101010111001","010010111010","101001011011","010100101011","101010010101","101100101010","101101010101","010101011100","010010111101","001000111101","100100011101","101010010101","101101001010","101101011010","010101101101","001010110110","100100111011","010010011011","011001010101","011010101001","011101010100","101101101010","010101101100","101010101101","010101010101","101100101001","101110010010","101110101001","010111010100","101011011010","010101011010","101010101011","010110010101","011101001001","011101100100","101110101010","010110110101","001010110110","101001010110","111001001101","101100100101","101101010010","101101101010","010110101101","001010101110","100100101111","010010010111","011001001011","011010100101","011010101100","101011010110","010101011101","010010011101","101001001101","110100010110","110110010101","010110101010","010110110101","001011011010","100101011011","010010101101","010110010101","011011001010","011011100100","101011101010","010011110101","001010110110","100101010110","101010101010","101101010100","101111010010","010111011001","001011101010","100101101101","010010101101","101010010101","101101001010","101110100101","010110110010","100110110101","010011010110","101010010111","010101000111","011010010011","011101001001","101101010101","010101101010","101001101011","010100101011","101010001011","110101000110","110110100011","010111001010","101011010110","010011011011","001001101011","100101001011","101010100101","101101010010","101101101001","010101110101","000101110110","100010110111","001001011011","010100101011","010101100101","010110110100","100111011010","010011101101","000101101101","100010110110","101010100110","110101010010","110110101001","010111010100","101011011010","100101011011","010010101011","011001010011","011100101001","011101100010","101110101001","010110110010","101010110101","010101010101","101100100101","110110010010","111011001001","011011010010","101011101001","010101101011","010010101011","101001010101","110100101001","110101010100","110110101010","100110110101","010010111010","101000111011","010010011011","101001001101","101010101010","101011010101","001011011010","100101011101","010001011110","101000101110","110010011010","110101010101","011010110010","011010111001","010010111010","101001011101","010100101101","101010010101","101101010010","101110101000","101110110100","010110111001","001011011010","100101011010","101101001010","110110100100","111011010001","011011101000","101101101010","010101101101","010100110101","011010010101","110101001010","110110101000","110111010100","011011011010","010101011011","001010011101","011000101011","101100010101","101101001010","101110010101","010110101010","101010101110","100100101110","110010001111","010100100111","011010010101","011010101010","101011010110","010101011101","001010011101"],_hijriBegin:1300,_hijriEnd:1600,_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,constructor:function(){
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
var d=this.toGregorian();
var dd=d.getDay();
return dd;
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
while(_10>=60){
this._hours++;
if(this._hours>=24){
this._date++;
this._hours-=24;
var _11=this.getDaysInIslamicMonth(this._month,this._year);
if(this._date>_11){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_11;
}
}
_10-=60;
}
this._minutes=_10;
},setSeconds:function(_12){
while(_12>=60){
this._minutes++;
if(this._minutes>=60){
this._hours++;
this._minutes-=60;
if(this._hours>=24){
this._date++;
this._hours-=24;
var _13=this.getDaysInIslamicMonth(this._month,this._year);
if(this._date>_13){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_13;
}
}
}
_12-=60;
}
this._seconds=_12;
},setMilliseconds:function(_14){
while(_14>=1000){
this.setSeconds++;
if(this.setSeconds>=60){
this._minutes++;
this.setSeconds-=60;
if(this._minutes>=60){
this._hours++;
this._minutes-=60;
if(this._hours>=24){
this._date++;
this._hours-=24;
var _15=this.getDaysInIslamicMonth(this._month,this._year);
if(this._date>_15){
this._month++;
if(this._month>=12){
this._year++;
this._month-=12;
}
this._date-=_15;
}
}
}
}
_14-=1000;
}
this._milliseconds=_14;
},toString:function(){
var x=new Date();
x.setHours(this._hours);
x.setMinutes(this._minutes);
x.setSeconds(this._seconds);
x.setMilliseconds(this._milliseconds);
return this._month+" "+this._date+" "+this._year+" "+x.toTimeString();
},toGregorian:function(){
var _16=this._year;
var _17=this._month;
var _18=this._date;
var _19=new Date();
var _1a=_18-1;
var _1b=new Date(1882,10,12,this._hours,this._minutes,this._seconds,this._milliseconds);
if(_16>=this._hijriBegin&&_16<=this._hijriEnd){
for(var y=0;y<_16-this._hijriBegin;y++){
for(var m=0;m<12;m++){
_1a+=parseInt(this._MONTH_LENGTH[y][m],10)+29;
}
}
for(m=0;m<_17;m++){
_1a+=parseInt(this._MONTH_LENGTH[_16-this._hijriBegin][m],10)+29;
}
_19=dd.add(_1b,"day",_1a);
}else{
var _1c=new dojox.date.islamic.Date(this._year,this._month,this._date,this._hours,this._minutes,this._seconds,this._milliseconds);
_19=new Date(_1c.toGregorian());
}
return _19;
},getDaysDiff:function(_1d,_1e){
var _1f=1000*60*60*24;
var _20=Math.abs(_1d.getTime()-_1e.getTime());
return Math.round(_20/_1f);
},fromGregorian:function(_21){
var _22=new Date(_21);
var _23=new Date(1882,10,12,0,0,0,0);
var _24=new Date(2174,10,25,23,59,59,999);
var _25=this.getDaysDiff(_22,_23);
if(_22-_23>=0&&_22-_24<=0){
var _26=1300;
for(var i=0;i<this._MONTH_LENGTH.length;i++,_26++){
for(var j=0;j<12;j++){
var _27=parseInt(this._MONTH_LENGTH[i][j],10)+29;
if(_25<=_27){
this._date=_25+1;
if(this._date>_27){
this._date=1;
j++;
}
if(j>11){
j=0;
_26++;
}
this._month=j;
this._year=_26;
this._hours=_22.getHours();
this._minutes=_22.getMinutes();
this._seconds=_22.getSeconds();
this._milliseconds=_22.getMilliseconds();
this._day=_22.getDay();
return this;
}
_25=parseInt(_25,10)-_27;
}
}
}else{
var _28=new dojox.date.islamic.Date(_22);
this._date=_28.getDate();
this._month=_28.getMonth();
this._year=_28.getFullYear();
this._hours=_21.getHours();
this._minutes=_21.getMinutes();
this._seconds=_21.getSeconds();
this._milliseconds=_21.getMilliseconds();
this._day=_21.getDay();
}
return this;
},valueOf:function(){
return (this.toGregorian()).valueOf();
},_yearStart:function(_29){
return (_29-1)*354+Math.floor((3+11*_29)/30);
},_monthStart:function(_2a,_2b){
return Math.ceil(29.5*_2b)+(_2a-1)*354+Math.floor((3+11*_2a)/30);
},_civilLeapYear:function(_2c){
return (14+11*_2c)%30<11;
},getDaysInIslamicMonth:function(_2d,_2e){
if(_2e>=this._hijriBegin&&_2e<=this._hijriEnd){
var pos=_2e-this._hijriBegin;
var _2f=0;
if(this._MONTH_LENGTH[pos].charAt(_2d)==1){
_2f=30;
}else{
_2f=29;
}
}else{
var _30=new dojox.date.islamic.Date();
_2f=_30.getDaysInIslamicMonth(_2d,_2e);
}
return _2f;
}});
_3.getDaysInIslamicMonth=function(_31){
return new _3().getDaysInIslamicMonth(_31.getMonth(),_31.getFullYear());
};
return _3;
});
