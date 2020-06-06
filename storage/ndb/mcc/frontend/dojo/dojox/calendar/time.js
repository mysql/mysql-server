//>>built
define("dojox/calendar/time",["dojo/_base/lang","dojo/date","dojo/cldr/supplemental","dojo/date/stamp"],function(_1,_2,_3,_4){
var _5={};
_5.newDate=function(_6,_7){
_7=_7||Date;
var d;
if(typeof (_6)=="number"){
return new _7(_6);
}else{
if(_6.getTime){
return new _7(_6.getTime());
}else{
if(_6.toGregorian){
d=_6.toGregorian();
if(_7!==Date){
d=new _7(d.getTime());
}
return d;
}else{
if(typeof _6=="string"){
d=_4.fromISOString(_6);
if(d===null){
throw new Error("Cannot parse date string ("+_6+"), specify a \"decodeDate\" function that translates this string into a Date object");
}else{
if(_7!==Date){
d=new _7(d.getTime());
}
}
return d;
}
}
}
}
};
_5.floorToDay=function(d,_8,_9){
_9=_9||Date;
if(!_8){
d=_5.newDate(d,_9);
}
d.setHours(0,0,0,0);
return d;
};
_5.floorToMonth=function(d,_a,_b){
_b=_b||Date;
if(!_a){
d=_5.newDate(d,_b);
}
d.setDate(1);
d.setHours(0,0,0,0);
return d;
};
_5.floorToWeek=function(d,_c,_d,_e,_f){
_c=_c||Date;
_d=_d||_2;
var fd=_e==undefined||_e<0?_3.getFirstDayOfWeek(_f):_e;
var day=d.getDay();
if(day==fd){
return d;
}
return _5.floorToDay(_d.add(d,"day",day>fd?-day+fd:-day+fd-7),true,_c);
};
_5.floor=function(_10,_11,_12,_13,_14){
var d=_5.floorToDay(_10,_13,_14);
switch(_11){
case "week":
return _5.floorToWeek(d,firstDayOfWeek,dateModule,locale);
case "minute":
d.setHours(_10.getHours());
d.setMinutes(Math.floor(_10.getMinutes()/_12)*_12);
break;
case "hour":
d.setHours(Math.floor(_10.getHours()/_12)*_12);
break;
}
return d;
};
_5.isStartOfDay=function(d,_15,_16){
_16=_16||_2;
return _16.compare(this.floorToDay(d,false,_15),d)==0;
};
_5.isToday=function(d,_17){
_17=_17||Date;
var _18=new _17();
return d.getFullYear()==_18.getFullYear()&&d.getMonth()==_18.getMonth()&&d.getDate()==_18.getDate();
};
_5.isOverlapping=function(_19,_1a,_1b,_1c,_1d,_1e){
if(_1a==null||_1c==null||_1b==null||_1d==null){
return false;
}
var cal=_19.dateModule;
if(_1e){
if(cal.compare(_1a,_1d)==1||cal.compare(_1c,_1b)==1){
return false;
}
}else{
if(cal.compare(_1a,_1d)!=-1||cal.compare(_1c,_1b)!=-1){
return false;
}
}
return true;
};
return _5;
});
