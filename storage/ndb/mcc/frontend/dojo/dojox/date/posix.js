//>>built
define("dojox/date/posix",["dojo/_base/kernel","dojo/date","dojo/date/locale","dojo/string","dojo/cldr/supplemental"],function(_1,_2,_3,_4,_5){
var _6=_1.getObject("date.posix",true,dojox);
_6.strftime=function(_7,_8,_9){
var _a=null;
var _b=function(s,n){
return _4.pad(s,n||2,_a||"0");
};
var _c=_3._getGregorianBundle(_9);
var $=function(_d){
switch(_d){
case "a":
return _3.getNames("days","abbr","format",_9)[_7.getDay()];
case "A":
return _3.getNames("days","wide","format",_9)[_7.getDay()];
case "b":
case "h":
return _3.getNames("months","abbr","format",_9)[_7.getMonth()];
case "B":
return _3.getNames("months","wide","format",_9)[_7.getMonth()];
case "c":
return _3.format(_7,{formatLength:"full",locale:_9});
case "C":
return _b(Math.floor(_7.getFullYear()/100));
case "d":
return _b(_7.getDate());
case "D":
return $("m")+"/"+$("d")+"/"+$("y");
case "e":
if(_a==null){
_a=" ";
}
return _b(_7.getDate());
case "f":
if(_a==null){
_a=" ";
}
return _b(_7.getMonth()+1);
case "g":
break;
case "G":
console.warn("unimplemented modifier 'G'");
break;
case "F":
return $("Y")+"-"+$("m")+"-"+$("d");
case "H":
return _b(_7.getHours());
case "I":
return _b(_7.getHours()%12||12);
case "j":
return _b(_3._getDayOfYear(_7),3);
case "k":
if(_a==null){
_a=" ";
}
return _b(_7.getHours());
case "l":
if(_a==null){
_a=" ";
}
return _b(_7.getHours()%12||12);
case "m":
return _b(_7.getMonth()+1);
case "M":
return _b(_7.getMinutes());
case "n":
return "\n";
case "p":
return _c["dayPeriods-format-wide-"+(_7.getHours()<12?"am":"pm")];
case "r":
return $("I")+":"+$("M")+":"+$("S")+" "+$("p");
case "R":
return $("H")+":"+$("M");
case "S":
return _b(_7.getSeconds());
case "t":
return "\t";
case "T":
return $("H")+":"+$("M")+":"+$("S");
case "u":
return String(_7.getDay()||7);
case "U":
return _b(_3._getWeekOfYear(_7));
case "V":
return _b(_6.getIsoWeekOfYear(_7));
case "W":
return _b(_3._getWeekOfYear(_7,1));
case "w":
return String(_7.getDay());
case "x":
return _3.format(_7,{selector:"date",formatLength:"full",locale:_9});
case "X":
return _3.format(_7,{selector:"time",formatLength:"full",locale:_9});
case "y":
return _b(_7.getFullYear()%100);
case "Y":
return String(_7.getFullYear());
case "z":
var _e=_7.getTimezoneOffset();
return (_e>0?"-":"+")+_b(Math.floor(Math.abs(_e)/60))+":"+_b(Math.abs(_e)%60);
case "Z":
return _2.getTimezoneName(_7);
case "%":
return "%";
}
};
var _f="",i=0,_10=0,_11=null;
while((_10=_8.indexOf("%",i))!=-1){
_f+=_8.substring(i,_10++);
switch(_8.charAt(_10++)){
case "_":
_a=" ";
break;
case "-":
_a="";
break;
case "0":
_a="0";
break;
case "^":
_11="upper";
break;
case "*":
_11="lower";
break;
case "#":
_11="swap";
break;
default:
_a=null;
_10--;
break;
}
var _12=$(_8.charAt(_10++));
switch(_11){
case "upper":
_12=_12.toUpperCase();
break;
case "lower":
_12=_12.toLowerCase();
break;
case "swap":
var _13=_12.toLowerCase();
var _14="";
var ch="";
for(var j=0;j<_12.length;j++){
ch=_12.charAt(j);
_14+=(ch==_13.charAt(j))?ch.toUpperCase():ch.toLowerCase();
}
_12=_14;
break;
default:
break;
}
_11=null;
_f+=_12;
i=_10;
}
_f+=_8.substring(i);
return _f;
};
_6.getStartOfWeek=function(_15,_16){
if(isNaN(_16)){
_16=_5.getFirstDayOfWeek?_5.getFirstDayOfWeek():0;
}
var _17=_16;
if(_15.getDay()>=_16){
_17-=_15.getDay();
}else{
_17-=(7-_15.getDay());
}
var _18=new Date(_15);
_18.setHours(0,0,0,0);
return _2.add(_18,"day",_17);
};
_6.setIsoWeekOfYear=function(_19,_1a){
if(!_1a){
return _19;
}
var _1b=_6.getIsoWeekOfYear(_19);
var _1c=_1a-_1b;
if(_1a<0){
var _1d=_6.getIsoWeeksInYear(_19);
_1c=(_1d+_1a+1)-_1b;
}
return _2.add(_19,"week",_1c);
};
_6.getIsoWeekOfYear=function(_1e){
var _1f=_6.getStartOfWeek(_1e,1);
var _20=new Date(_1e.getFullYear(),0,4);
_20=_6.getStartOfWeek(_20,1);
var _21=_1f.getTime()-_20.getTime();
if(_21<0){
return _6.getIsoWeeksInYear(_1f);
}
return Math.ceil(_21/604800000)+1;
};
_6.getIsoWeeksInYear=function(_22){
function p(y){
return y+Math.floor(y/4)-Math.floor(y/100)+Math.floor(y/400);
};
var y=_22.getFullYear();
return (p(y)%7==4||p(y-1)%7==3)?53:52;
};
return _6;
});
