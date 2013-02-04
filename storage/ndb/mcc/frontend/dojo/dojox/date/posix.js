//>>built
define("dojox/date/posix",["dojo/_base/kernel","dojo/date","dojo/date/locale","dojo/string","dojo/cldr/supplemental"],function(_1,_2,_3,_4,_5){
_1.getObject("date.posix",true,dojox);
dojox.date.posix.strftime=function(_6,_7,_8){
var _9=null;
var _a=function(s,n){
return _4.pad(s,n||2,_9||"0");
};
var _b=_3._getGregorianBundle(_8);
var $=function(_c){
switch(_c){
case "a":
return _3.getNames("days","abbr","format",_8)[_6.getDay()];
case "A":
return _3.getNames("days","wide","format",_8)[_6.getDay()];
case "b":
case "h":
return _3.getNames("months","abbr","format",_8)[_6.getMonth()];
case "B":
return _3.getNames("months","wide","format",_8)[_6.getMonth()];
case "c":
return _3.format(_6,{formatLength:"full",locale:_8});
case "C":
return _a(Math.floor(_6.getFullYear()/100));
case "d":
return _a(_6.getDate());
case "D":
return $("m")+"/"+$("d")+"/"+$("y");
case "e":
if(_9==null){
_9=" ";
}
return _a(_6.getDate());
case "f":
if(_9==null){
_9=" ";
}
return _a(_6.getMonth()+1);
case "g":
break;
case "G":
console.warn("unimplemented modifier 'G'");
break;
case "F":
return $("Y")+"-"+$("m")+"-"+$("d");
case "H":
return _a(_6.getHours());
case "I":
return _a(_6.getHours()%12||12);
case "j":
return _a(_3._getDayOfYear(_6),3);
case "k":
if(_9==null){
_9=" ";
}
return _a(_6.getHours());
case "l":
if(_9==null){
_9=" ";
}
return _a(_6.getHours()%12||12);
case "m":
return _a(_6.getMonth()+1);
case "M":
return _a(_6.getMinutes());
case "n":
return "\n";
case "p":
return _b["dayPeriods-format-wide-"+(_6.getHours()<12?"am":"pm")];
case "r":
return $("I")+":"+$("M")+":"+$("S")+" "+$("p");
case "R":
return $("H")+":"+$("M");
case "S":
return _a(_6.getSeconds());
case "t":
return "\t";
case "T":
return $("H")+":"+$("M")+":"+$("S");
case "u":
return String(_6.getDay()||7);
case "U":
return _a(_3._getWeekOfYear(_6));
case "V":
return _a(dojox.date.posix.getIsoWeekOfYear(_6));
case "W":
return _a(_3._getWeekOfYear(_6,1));
case "w":
return String(_6.getDay());
case "x":
return _3.format(_6,{selector:"date",formatLength:"full",locale:_8});
case "X":
return _3.format(_6,{selector:"time",formatLength:"full",locale:_8});
case "y":
return _a(_6.getFullYear()%100);
case "Y":
return String(_6.getFullYear());
case "z":
var _d=_6.getTimezoneOffset();
return (_d>0?"-":"+")+_a(Math.floor(Math.abs(_d)/60))+":"+_a(Math.abs(_d)%60);
case "Z":
return _2.getTimezoneName(_6);
case "%":
return "%";
}
};
var _e="",i=0,_f=0,_10=null;
while((_f=_7.indexOf("%",i))!=-1){
_e+=_7.substring(i,_f++);
switch(_7.charAt(_f++)){
case "_":
_9=" ";
break;
case "-":
_9="";
break;
case "0":
_9="0";
break;
case "^":
_10="upper";
break;
case "*":
_10="lower";
break;
case "#":
_10="swap";
break;
default:
_9=null;
_f--;
break;
}
var _11=$(_7.charAt(_f++));
switch(_10){
case "upper":
_11=_11.toUpperCase();
break;
case "lower":
_11=_11.toLowerCase();
break;
case "swap":
var _12=_11.toLowerCase();
var _13="";
var ch="";
for(var j=0;j<_11.length;j++){
ch=_11.charAt(j);
_13+=(ch==_12.charAt(j))?ch.toUpperCase():ch.toLowerCase();
}
_11=_13;
break;
default:
break;
}
_10=null;
_e+=_11;
i=_f;
}
_e+=_7.substring(i);
return _e;
};
dojox.date.posix.getStartOfWeek=function(_14,_15){
if(isNaN(_15)){
_15=_5.getFirstDayOfWeek?_5.getFirstDayOfWeek():0;
}
var _16=_15;
if(_14.getDay()>=_15){
_16-=_14.getDay();
}else{
_16-=(7-_14.getDay());
}
var _17=new Date(_14);
_17.setHours(0,0,0,0);
return _2.add(_17,"day",_16);
};
dojox.date.posix.setIsoWeekOfYear=function(_18,_19){
if(!_19){
return _18;
}
var _1a=dojox.date.posix.getIsoWeekOfYear(_18);
var _1b=_19-_1a;
if(_19<0){
var _1c=dojox.date.posix.getIsoWeeksInYear(_18);
_1b=(_1c+_19+1)-_1a;
}
return _2.add(_18,"week",_1b);
};
dojox.date.posix.getIsoWeekOfYear=function(_1d){
var _1e=dojox.date.posix.getStartOfWeek(_1d,1);
var _1f=new Date(_1d.getFullYear(),0,4);
_1f=dojox.date.posix.getStartOfWeek(_1f,1);
var _20=_1e.getTime()-_1f.getTime();
if(_20<0){
return dojox.date.posix.getIsoWeeksInYear(_1e);
}
return Math.ceil(_20/604800000)+1;
};
dojox.date.posix.getIsoWeeksInYear=function(_21){
function p(y){
return y+Math.floor(y/4)-Math.floor(y/100)+Math.floor(y/400);
};
var y=_21.getFullYear();
return (p(y)%7==4||p(y-1)%7==3)?53:52;
};
return dojox.date.posix;
});
