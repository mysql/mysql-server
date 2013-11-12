//>>built
define("dojox/date/hebrew/numerals",["dojo/_base/kernel","dojo/_base/array"],function(_1){
_1.getObject("date.hebrew.numerals",true,dojox);
_1.experimental("dojox.date.hebrew.numerals");
var _2="אבגדהוזחט";
var _3="יכלמנסעפצ";
var _4="קרשת";
var _5=function(_6,_7){
_6=_6.replace("יה","טו").replace("יו","טז");
if(!_7){
var _8=_6.length;
if(_8>1){
_6=_6.substr(0,_8-1)+"\""+_6.charAt(_8-1);
}else{
_6+="׳";
}
}
return _6;
};
var _9=function(_a){
var _b=0;
_1.forEach(_a,function(ch){
var i;
if((i=_2.indexOf(ch))!=-1){
_b+=++i;
}else{
if((i=_3.indexOf(ch))!=-1){
_b+=10*++i;
}else{
if((i=_4.indexOf(ch))!=-1){
_b+=100*++i;
}
}
}
});
return _b;
};
var _c=function(_d){
var _e="",n=4,j=9;
while(_d){
if(_d>=n*100){
_e+=_4.charAt(n-1);
_d-=n*100;
continue;
}else{
if(n>1){
n--;
continue;
}else{
if(_d>=j*10){
_e+=_3.charAt(j-1);
_d-=j*10;
}else{
if(j>1){
j--;
continue;
}else{
if(_d>0){
_e+=_2.charAt(_d-1);
_d=0;
}
}
}
}
}
}
return _e;
};
dojox.date.hebrew.numerals.getYearHebrewLetters=function(_f){
var rem=_f%1000;
return _5(_c(rem));
};
dojox.date.hebrew.numerals.parseYearHebrewLetters=function(_10){
return _9(_10)+5000;
};
dojox.date.hebrew.numerals.getDayHebrewLetters=function(day,_11){
return _5(_c(day),_11);
};
dojox.date.hebrew.numerals.parseDayHebrewLetters=function(day){
return _9(day);
};
dojox.date.hebrew.numerals.getMonthHebrewLetters=function(_12){
return _5(_c(_12+1));
};
dojox.date.hebrew.numerals.parseMonthHebrewLetters=function(_13){
var _14=dojox.date.hebrew.numerals.parseDayHebrewLetters(_13)-1;
if(_14==-1||_14>12){
throw new Error("The month name is incorrect , month = "+_14);
}
return _14;
};
return dojox.date.hebrew.numerals;
});
