//>>built
define("dojox/date/hebrew/numerals",["../..","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
var _4=_2.getObject("date.hebrew.numerals",true,_1);
var _5="אבגדהוזחט";
var _6="יכלמנסעפצ";
var _7="קרשת";
var _8=function(_9,_a){
_9=_9.replace("יה","טו").replace("יו","טז");
if(!_a){
var _b=_9.length;
if(_b>1){
_9=_9.substr(0,_b-1)+"\""+_9.charAt(_b-1);
}else{
_9+="׳";
}
}
return _9;
};
var _c=function(_d){
var _e=0;
_3.forEach(_d,function(ch){
var i;
if((i=_5.indexOf(ch))!=-1){
_e+=++i;
}else{
if((i=_6.indexOf(ch))!=-1){
_e+=10*++i;
}else{
if((i=_7.indexOf(ch))!=-1){
_e+=100*++i;
}
}
}
});
return _e;
};
var _f=function(num){
var str="",n=4,j=9;
while(num){
if(num>=n*100){
str+=_7.charAt(n-1);
num-=n*100;
continue;
}else{
if(n>1){
n--;
continue;
}else{
if(num>=j*10){
str+=_6.charAt(j-1);
num-=j*10;
}else{
if(j>1){
j--;
continue;
}else{
if(num>0){
str+=_5.charAt(num-1);
num=0;
}
}
}
}
}
}
return str;
};
_4.getYearHebrewLetters=function(_10){
var rem=_10%1000;
return _8(_f(rem));
};
_4.parseYearHebrewLetters=function(_11){
return _c(_11)+5000;
};
_4.getDayHebrewLetters=function(day,_12){
return _8(_f(day),_12);
};
_4.parseDayHebrewLetters=function(day){
return _c(day);
};
_4.getMonthHebrewLetters=function(_13){
return _8(_f(_13+1));
};
_4.parseMonthHebrewLetters=function(_14){
var _15=_4.parseDayHebrewLetters(_14)-1;
if(_15==-1||_15>12){
throw new Error("The month name is incorrect , month = "+_15);
}
return _15;
};
return _4;
});
