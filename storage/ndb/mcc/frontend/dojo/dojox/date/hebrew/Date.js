//>>built
define("dojox/date/hebrew/Date",["dojo/_base/kernel","dojo/_base/declare","./numerals"],function(_1,_2,_3){
_1.getObject("date.hebrew.Date",true,dojox);
_1.experimental("dojox.date.hebrew.Date");
_1.declare("dojox.date.hebrew.Date",null,{_MONTH_LENGTH:[[30,30,30],[29,29,30],[29,30,30],[29,29,29],[30,30,30],[30,30,30],[29,29,29],[30,30,30],[29,29,29],[30,30,30],[29,29,29],[30,30,30],[29,29,29]],_MONTH_START:[[0,0,0],[30,30,30],[59,59,60],[88,89,90],[117,118,119],[147,148,149],[147,148,149],[176,177,178],[206,207,208],[235,236,237],[265,266,267],[294,295,296],[324,325,326],[353,354,355]],_LEAP_MONTH_START:[[0,0,0],[30,30,30],[59,59,60],[88,89,90],[117,118,119],[147,148,149],[177,178,179],[206,207,208],[236,237,238],[265,266,267],[295,296,297],[324,325,326],[354,355,356],[383,384,385]],_GREGORIAN_MONTH_COUNT:[[31,31,0,0],[28,29,31,31],[31,31,59,60],[30,30,90,91],[31,31,120,121],[30,30,151,152],[31,31,181,182],[31,31,212,213],[30,30,243,244],[31,31,273,274],[30,30,304,305],[31,31,334,335]],_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,constructor:function(){
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
if(this._month>12){
console.warn("the month is incorrect , set 0  "+this._month+"   "+this._year);
this._month=0;
}
this._hours+=arguments[3]||0;
this._minutes+=arguments[4]||0;
this._seconds+=arguments[5]||0;
this._milliseconds+=arguments[6]||0;
}
}
}
this._setDay();
},getDate:function(){
return this._date;
},getDateLocalized:function(_6){
return (_6||_1.locale).match(/^he(?:-.+)?$/)?_3.getDayHebrewLetters(this._date):this.getDate();
},getMonth:function(){
return this._month;
},getFullYear:function(){
return this._year;
},getHours:function(){
return this._hours;
},getMinutes:function(){
return this._minutes;
},getSeconds:function(){
return this._seconds;
},getMilliseconds:function(){
return this._milliseconds;
},setDate:function(_7){
_7=+_7;
var _8;
if(_7>0){
while(_7>(_8=this.getDaysInHebrewMonth(this._month,this._year))){
_7-=_8;
this._month++;
if(this._month>=13){
this._year++;
this._month-=13;
}
}
}else{
while(_7<=0){
_8=this.getDaysInHebrewMonth((this._month-1)>=0?(this._month-1):12,((this._month-1)>=0)?this._year:this._year-1);
this._month--;
if(this._month<0){
this._year--;
this._month+=13;
}
_7+=_8;
}
}
this._date=_7;
this._setDay();
return this;
},setFullYear:function(_9,_a,_b){
this._year=_9=+_9;
if(!this.isLeapYear(_9)&&this._month==5){
this._month++;
}
if(_a!==undefined){
this.setMonth(_a);
}
if(_b!==undefined){
this.setDate(_b);
}
var _c=this.getDaysInHebrewMonth(this._month,this._year);
if(_c<this._date){
this._date=_c;
}
this._setDay();
return this;
},setMonth:function(_d){
_d=+_d;
if(!this.isLeapYear(this._year)&&_d==5){
_d++;
}
if(_d>=0){
while(_d>12){
this._year++;
_d-=13;
if(!this.isLeapYear(this._year)&&_d>=5){
_d++;
}
}
}else{
while(_d<0){
this._year--;
_d+=(!this.isLeapYear(this._year)&&_d<-7)?12:13;
}
}
this._month=_d;
var _e=this.getDaysInHebrewMonth(this._month,this._year);
if(_e<this._date){
this._date=_e;
}
this._setDay();
return this;
},setHours:function(){
var _f=arguments.length;
var _10=0;
if(_f>=1){
_10+=+arguments[0];
}
if(_f>=2){
this._minutes+=+arguments[1];
}
if(_f>=3){
this._seconds+=+arguments[2];
}
if(_f==4){
this._milliseconds+=+arguments[3];
}
while(_10>=24){
this._date++;
var _11=this.getDaysInHebrewMonth(this._month,this._year);
if(this._date>_11){
this._month++;
if(!this.isLeapYear(this._year)&&this._month==5){
this._month++;
}
if(this._month>=13){
this._year++;
this._month-=13;
}
this._date-=_11;
}
_10-=24;
}
this._hours=_10;
this._setDay();
return this;
},_addMinutes:function(_12){
_12+=this._minutes;
this.setMinutes(_12);
this.setHours(this._hours+parseInt(_12/60));
return this;
},_addSeconds:function(_13){
_13+=this._seconds;
this.setSeconds(_13);
this._addMinutes(parseInt(_13/60));
return this;
},_addMilliseconds:function(_14){
_14+=this._milliseconds;
this.setMilliseconds(_14);
this._addSeconds(parseInt(_14/1000));
return this;
},setMinutes:function(_15){
this._minutes=_15%60;
return this;
},setSeconds:function(_16){
this._seconds=_16%60;
return this;
},setMilliseconds:function(_17){
this._milliseconds=_17%1000;
return this;
},_setDay:function(){
var day=this._startOfYear(this._year);
if(this._month!=0){
day+=(this.isLeapYear(this._year)?this._LEAP_MONTH_START:this._MONTH_START)[this._month||0][this._yearType(this._year)];
}
day+=this._date-1;
this._day=(day+1)%7;
},toString:function(){
return this._date+", "+this._month+", "+this._year+"  "+this._hours+":"+this._minutes+":"+this._seconds;
},getDaysInHebrewMonth:function(_18,_19){
var _1a=(_18==1||_18==2)?this._yearType(_19):0;
return (!this.isLeapYear(this._year)&&_18==5)?0:this._MONTH_LENGTH[_18][_1a];
},_yearType:function(_1b){
var _1c=this._handleGetYearLength(Number(_1b));
if(_1c>380){
_1c-=30;
}
var _1d=_1c-353;
if(_1d<0||_1d>2){
throw new Error("Illegal year length "+_1c+" in year "+_1b);
}
return _1d;
},_handleGetYearLength:function(_1e){
return this._startOfYear(_1e+1)-this._startOfYear(_1e);
},_startOfYear:function(_1f){
var _20=Math.floor((235*_1f-234)/19),_21=_20*(12*1080+793)+11*1080+204,day=_20*29+Math.floor(_21/(24*1080));
_21%=24*1080;
var wd=day%7;
if(wd==2||wd==4||wd==6){
day+=1;
wd=day%7;
}
if(wd==1&&_21>15*1080+204&&!this.isLeapYear(_1f)){
day+=2;
}else{
if(wd==0&&_21>21*1080+589&&this.isLeapYear(_1f-1)){
day+=1;
}
}
return day;
},isLeapYear:function(_22){
var x=(_22*12+17)%19;
return x>=((x<0)?-7:12);
},fromGregorian:function(_23){
var _24=(!isNaN(_23))?this._computeHebrewFields(_23):NaN;
this._year=(!isNaN(_23))?_24[0]:NaN;
this._month=(!isNaN(_23))?_24[1]:NaN;
this._date=(!isNaN(_23))?_24[2]:NaN;
this._hours=_23.getHours();
this._milliseconds=_23.getMilliseconds();
this._minutes=_23.getMinutes();
this._seconds=_23.getSeconds();
if(!isNaN(_23)){
this._setDay();
}
return this;
},_computeHebrewFields:function(_25){
var _26=this._getJulianDayFromGregorianDate(_25),d=_26-347997,m=Math.floor((d*24*1080)/(29*24*1080+12*1080+793)),_27=Math.floor((19*m+234)/235)+1,ys=this._startOfYear(_27),_28=(d-ys);
while(_28<1){
_27--;
ys=this._startOfYear(_27);
_28=d-ys;
}
var _29=this._yearType(_27),_2a=this.isLeapYear(_27)?this._LEAP_MONTH_START:this._MONTH_START,_2b=0;
while(_28>_2a[_2b][_29]){
_2b++;
}
_2b--;
var _2c=_28-_2a[_2b][_29];
return [_27,_2b,_2c];
},toGregorian:function(){
var _2d=this._year||0,_2e=this._month||0,_2f=this._date||0,day=this._startOfYear(_2d);
if(_2e!=0){
day+=(this.isLeapYear(_2d)?this._LEAP_MONTH_START:this._MONTH_START)[_2e][this._yearType(_2d)];
}
var _30=(_2f+day+347997),_31=_30-1721426;
var rem=[];
var _32=this._floorDivide(_31,146097,rem),_33=this._floorDivide(rem[0],36524,rem),n4=this._floorDivide(rem[0],1461,rem),n1=this._floorDivide(rem[0],365,rem),_34=400*_32+100*_33+4*n4+n1,_35=rem[0];
if(_33==4||n1==4){
_35=365;
}else{
++_34;
}
var _36=!(_34%4)&&(_34%100||!(_34%400)),_37=0,_38=_36?60:59;
if(_35>=_38){
_37=_36?1:2;
}
var _39=Math.floor((12*(_35+_37)+6)/367);
var _3a=_35-this._GREGORIAN_MONTH_COUNT[_39][_36?3:2]+1;
return new Date(_34,_39,_3a,this._hours,this._minutes,this._seconds,this._milliseconds);
},_floorDivide:function(_3b,_3c,_3d){
if(_3b>=0){
_3d[0]=(_3b%_3c);
return Math.floor(_3b/_3c);
}
var _3e=Math.floor(_3b/_3c);
_3d[0]=_3b-(_3e*_3c);
return _3e;
},getDay:function(){
var _3f=this._year,_40=this._month,_41=this._date,day=this._startOfYear(_3f);
if(_40!=0){
day+=(this.isLeapYear(_3f)?this._LEAP_MONTH_START:this._MONTH_START)[_40][this._yearType(_3f)];
}
day+=_41-1;
return (day+1)%7;
},_getJulianDayFromGregorianDate:function(_42){
var _43=_42.getFullYear(),_44=_42.getMonth(),d=_42.getDate(),_45=!(_43%4)&&(_43%100||!(_43%400)),y=_43-1;
var _46=365*y+Math.floor(y/4)-Math.floor(y/100)+Math.floor(y/400)+1721426-1;
if(_44>0){
_46+=this._GREGORIAN_MONTH_COUNT[_44][_45?3:2];
}
_46+=d;
return _46;
}});
dojox.date.hebrew.Date.prototype.valueOf=function(){
return this.toGregorian().valueOf();
};
return dojox.date.hebrew.Date;
});
