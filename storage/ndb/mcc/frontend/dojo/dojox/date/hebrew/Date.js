//>>built
define("dojox/date/hebrew/Date",["dojo/_base/lang","dojo/_base/declare","./numerals"],function(_1,_2,_3){
var _4=_2("dojox.date.hebrew.Date",null,{_MONTH_LENGTH:[[30,30,30],[29,29,30],[29,30,30],[29,29,29],[30,30,30],[30,30,30],[29,29,29],[30,30,30],[29,29,29],[30,30,30],[29,29,29],[30,30,30],[29,29,29]],_MONTH_START:[[0,0,0],[30,30,30],[59,59,60],[88,89,90],[117,118,119],[147,148,149],[147,148,149],[176,177,178],[206,207,208],[235,236,237],[265,266,267],[294,295,296],[324,325,326],[353,354,355]],_LEAP_MONTH_START:[[0,0,0],[30,30,30],[59,59,60],[88,89,90],[117,118,119],[147,148,149],[177,178,179],[206,207,208],[236,237,238],[265,266,267],[295,296,297],[324,325,326],[354,355,356],[383,384,385]],_GREGORIAN_MONTH_COUNT:[[31,31,0,0],[28,29,31,31],[31,31,59,60],[30,30,90,91],[31,31,120,121],[30,30,151,152],[31,31,181,182],[31,31,212,213],[30,30,243,244],[31,31,273,274],[30,30,304,305],[31,31,334,335]],_date:0,_month:0,_year:0,_hours:0,_minutes:0,_seconds:0,_milliseconds:0,_day:0,constructor:function(){
var _5=arguments.length;
if(!_5){
this.fromGregorian(new Date());
}else{
if(_5==1){
var _6=arguments[0];
if(typeof _6=="number"){
_6=new Date(_6);
}
if(_6 instanceof Date){
this.fromGregorian(_6);
}else{
if(_6==""){
this._year=this._month=this._date=this._hours=this._minutes=this._seconds=this._milliseconds=NaN;
}else{
this._year=_6._year;
this._month=_6._month;
this._date=_6._date;
this._hours=_6._hours;
this._minutes=_6._minutes;
this._seconds=_6._seconds;
this._milliseconds=_6._milliseconds;
}
}
}else{
if(_5>=3){
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
},getDateLocalized:function(_7){
return (_7||dojo.locale).match(/^he(?:-.+)?$/)?_3.getDayHebrewLetters(this._date):this.getDate();
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
},setDate:function(_8){
_8=+_8;
var _9;
if(_8>0){
while(_8>(_9=this.getDaysInHebrewMonth(this._month,this._year))){
_8-=_9;
this._month++;
if(this._month>=13){
this._year++;
this._month-=13;
}
}
}else{
while(_8<=0){
_9=this.getDaysInHebrewMonth((this._month-1)>=0?(this._month-1):12,((this._month-1)>=0)?this._year:this._year-1);
this._month--;
if(this._month<0){
this._year--;
this._month+=13;
}
_8+=_9;
}
}
this._date=_8;
this._setDay();
return this;
},setFullYear:function(_a,_b,_c){
this._year=_a=+_a;
if(!this.isLeapYear(_a)&&this._month==5){
this._month++;
}
if(_b!==undefined){
this.setMonth(_b);
}
if(_c!==undefined){
this.setDate(_c);
}
var _d=this.getDaysInHebrewMonth(this._month,this._year);
if(_d<this._date){
this._date=_d;
}
this._setDay();
return this;
},setMonth:function(_e){
_e=+_e;
if(!this.isLeapYear(this._year)&&_e==5){
_e++;
}
if(_e>=0){
while(_e>12){
this._year++;
_e-=13;
if(!this.isLeapYear(this._year)&&_e>=5){
_e++;
}
}
}else{
while(_e<0){
this._year--;
_e+=(!this.isLeapYear(this._year)&&_e<-7)?12:13;
}
}
this._month=_e;
var _f=this.getDaysInHebrewMonth(this._month,this._year);
if(_f<this._date){
this._date=_f;
}
this._setDay();
return this;
},setHours:function(){
var _10=arguments.length;
var _11=0;
if(_10>=1){
_11=+arguments[0];
}
if(_10>=2){
this._minutes=+arguments[1];
}
if(_10>=3){
this._seconds=+arguments[2];
}
if(_10==4){
this._milliseconds=+arguments[3];
}
while(_11>=24){
this._date++;
var _12=this.getDaysInHebrewMonth(this._month,this._year);
if(this._date>_12){
this._month++;
if(!this.isLeapYear(this._year)&&this._month==5){
this._month++;
}
if(this._month>=13){
this._year++;
this._month-=13;
}
this._date-=_12;
}
_11-=24;
}
this._hours=_11;
this._setDay();
return this;
},_addMinutes:function(_13){
_13+=this._minutes;
this.setMinutes(_13);
this.setHours(this._hours+parseInt(_13/60));
return this;
},_addSeconds:function(_14){
_14+=this._seconds;
this.setSeconds(_14);
this._addMinutes(parseInt(_14/60));
return this;
},_addMilliseconds:function(_15){
_15+=this._milliseconds;
this.setMilliseconds(_15);
this._addSeconds(parseInt(_15/1000));
return this;
},setMinutes:function(_16){
this._minutes=_16%60;
return this;
},setSeconds:function(_17){
this._seconds=_17%60;
return this;
},setMilliseconds:function(_18){
this._milliseconds=_18%1000;
return this;
},_setDay:function(){
var day=this._startOfYear(this._year);
if(this._month!=0){
day+=(this.isLeapYear(this._year)?this._LEAP_MONTH_START:this._MONTH_START)[this._month||0][this._yearType(this._year)];
}
day+=this._date-1;
this._day=(day+1)%7;
},toString:function(){
return isNaN(this._date)?"Invalid Date":this._date+", "+this._month+", "+this._year+"  "+this._hours+":"+this._minutes+":"+this._seconds;
},getDaysInHebrewMonth:function(_19,_1a){
var _1b=(_19==1||_19==2)?this._yearType(_1a):0;
return (!this.isLeapYear(this._year)&&_19==5)?0:this._MONTH_LENGTH[_19][_1b];
},_yearType:function(_1c){
var _1d=this._handleGetYearLength(Number(_1c));
if(_1d>380){
_1d-=30;
}
var _1e=_1d-353;
if(_1e<0||_1e>2){
throw new Error("Illegal year length "+_1d+" in year "+_1c);
}
return _1e;
},_handleGetYearLength:function(_1f){
return this._startOfYear(_1f+1)-this._startOfYear(_1f);
},_startOfYear:function(_20){
var _21=Math.floor((235*_20-234)/19),_22=_21*(12*1080+793)+11*1080+204,day=_21*29+Math.floor(_22/(24*1080));
_22%=24*1080;
var wd=day%7;
if(wd==2||wd==4||wd==6){
day+=1;
wd=day%7;
}
if(wd==1&&_22>15*1080+204&&!this.isLeapYear(_20)){
day+=2;
}else{
if(wd==0&&_22>21*1080+589&&this.isLeapYear(_20-1)){
day+=1;
}
}
return day;
},isLeapYear:function(_23){
var x=(_23*12+17)%19;
return x>=((x<0)?-7:12);
},fromGregorian:function(_24){
var _25=(!isNaN(_24))?this._computeHebrewFields(_24):NaN;
this._year=(!isNaN(_24))?_25[0]:NaN;
this._month=(!isNaN(_24))?_25[1]:NaN;
this._date=(!isNaN(_24))?_25[2]:NaN;
this._hours=_24.getHours();
this._milliseconds=_24.getMilliseconds();
this._minutes=_24.getMinutes();
this._seconds=_24.getSeconds();
if(!isNaN(_24)){
this._setDay();
}
return this;
},_computeHebrewFields:function(_26){
var _27=this._getJulianDayFromGregorianDate(_26),d=_27-347997,m=Math.floor((d*24*1080)/(29*24*1080+12*1080+793)),_28=Math.floor((19*m+234)/235)+1,ys=this._startOfYear(_28),_29=(d-ys);
while(_29<1){
_28--;
ys=this._startOfYear(_28);
_29=d-ys;
}
var _2a=this._yearType(_28),_2b=this.isLeapYear(_28)?this._LEAP_MONTH_START:this._MONTH_START,_2c=0;
while(_29>_2b[_2c][_2a]){
_2c++;
}
_2c--;
var _2d=_29-_2b[_2c][_2a];
return [_28,_2c,_2d];
},toGregorian:function(){
var _2e=this._year||0,_2f=this._month||0,_30=this._date||0,day=this._startOfYear(_2e);
if(_2f!=0){
day+=(this.isLeapYear(_2e)?this._LEAP_MONTH_START:this._MONTH_START)[_2f][this._yearType(_2e)];
}
var _31=(_30+day+347997),_32=_31-1721426;
var rem=[];
var _33=this._floorDivide(_32,146097,rem),_34=this._floorDivide(rem[0],36524,rem),n4=this._floorDivide(rem[0],1461,rem),n1=this._floorDivide(rem[0],365,rem),_35=400*_33+100*_34+4*n4+n1,_36=rem[0];
if(_34==4||n1==4){
_36=365;
}else{
++_35;
}
var _37=!(_35%4)&&(_35%100||!(_35%400)),_38=0,_39=_37?60:59;
if(_36>=_39){
_38=_37?1:2;
}
var _3a=Math.floor((12*(_36+_38)+6)/367);
var _3b=_36-this._GREGORIAN_MONTH_COUNT[_3a][_37?3:2]+1;
return new Date(_35,_3a,_3b,this._hours,this._minutes,this._seconds,this._milliseconds);
},_floorDivide:function(_3c,_3d,_3e){
if(_3c>=0){
_3e[0]=(_3c%_3d);
return Math.floor(_3c/_3d);
}
var _3f=Math.floor(_3c/_3d);
_3e[0]=_3c-(_3f*_3d);
return _3f;
},getDay:function(){
var _40=this._year,_41=this._month,_42=this._date,day=this._startOfYear(_40);
if(_41!=0){
day+=(this.isLeapYear(_40)?this._LEAP_MONTH_START:this._MONTH_START)[_41][this._yearType(_40)];
}
day+=_42-1;
return (day+1)%7;
},_getJulianDayFromGregorianDate:function(_43){
var _44=_43.getFullYear(),_45=_43.getMonth(),d=_43.getDate(),_46=!(_44%4)&&(_44%100||!(_44%400)),y=_44-1;
var _47=365*y+Math.floor(y/4)-Math.floor(y/100)+Math.floor(y/400)+1721426-1;
if(_45>0){
_47+=this._GREGORIAN_MONTH_COUNT[_45][_46?3:2];
}
_47+=d;
return _47;
}});
_4.prototype.valueOf=function(){
return this.toGregorian().valueOf();
};
return _4;
});
