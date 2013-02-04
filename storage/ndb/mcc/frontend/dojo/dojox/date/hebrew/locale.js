//>>built
define("dojox/date/hebrew/locale",["dojo/main","dojo/date","dojo/i18n","dojo/regexp","dojo/string","./Date","./numerals","dojo/i18n!dojo/cldr/nls/hebrew"],function(_1,dd,_2,_3,_4,_5,_6){
_1.getObject("date.hebrew.locale",true,dojox);
_1.experimental("dojox.date.hebrew.locale");
_1.requireLocalization("dojo.cldr","hebrew");
function _7(_8,_9,_a,_b,_c){
return _c.replace(/([a-z])\1*/ig,function(_d){
var s,_e;
var c=_d.charAt(0);
var l=_d.length;
var _f=["abbr","wide","narrow"];
switch(c){
case "y":
if(_a.match(/^he(?:-.+)?$/)){
s=_6.getYearHebrewLetters(_8.getFullYear());
}else{
s=String(_8.getFullYear());
}
break;
case "M":
var m=_8.getMonth();
if(l<3){
if(!_8.isLeapYear(_8.getFullYear())&&m>5){
m--;
}
if(_a.match(/^he(?:-.+)?$/)){
s=_6.getMonthHebrewLetters(m);
}else{
s=m+1;
_e=true;
}
}else{
var _10=dojox.date.hebrew.locale.getNames("months",_f[l-3],"format",_a,_8);
s=_10[m];
}
break;
case "d":
if(_a.match(/^he(?:-.+)?$/)){
s=_8.getDateLocalized(_a);
}else{
s=_8.getDate();
_e=true;
}
break;
case "E":
var d=_8.getDay();
if(l<3){
s=d+1;
_e=true;
}else{
var _11=["days","format",_f[l-3]].join("-");
s=_9[_11][d];
}
break;
case "a":
var _12=(_8.getHours()<12)?"am":"pm";
s=_9["dayPeriods-format-wide-"+_12];
break;
case "h":
case "H":
case "K":
case "k":
var h=_8.getHours();
switch(c){
case "h":
s=(h%12)||12;
break;
case "H":
s=h;
break;
case "K":
s=(h%12);
break;
case "k":
s=h||24;
break;
}
_e=true;
break;
case "m":
s=_8.getMinutes();
_e=true;
break;
case "s":
s=_8.getSeconds();
_e=true;
break;
case "S":
s=Math.round(_8.getMilliseconds()*Math.pow(10,l-3));
_e=true;
break;
case "z":
s="";
break;
default:
throw new Error("dojox.date.hebrew.locale.formatPattern: invalid pattern char: "+_c);
}
if(_e){
s=_4.pad(s,l);
}
return s;
});
};
dojox.date.hebrew.locale.format=function(_13,_14){
_14=_14||{};
var _15=_2.normalizeLocale(_14.locale);
var _16=_14.formatLength||"short";
var _17=dojox.date.hebrew.locale._getHebrewBundle(_15);
var str=[];
var _18=_1.hitch(this,_7,_13,_17,_15,_14.fullYear);
if(_14.selector=="year"){
var _19=_13.getFullYear();
return _15.match(/^he(?:-.+)?$/)?_6.getYearHebrewLetters(_19):_19;
}
if(_14.selector!="time"){
var _1a=_14.datePattern||_17["dateFormat-"+_16];
if(_1a){
str.push(_1b(_1a,_18));
}
}
if(_14.selector!="date"){
var _1c=_14.timePattern||_17["timeFormat-"+_16];
if(_1c){
str.push(_1b(_1c,_18));
}
}
var _1d=str.join(" ");
return _1d;
};
dojox.date.hebrew.locale.regexp=function(_1e){
return dojox.date.hebrew.locale._parseInfo(_1e).regexp;
};
dojox.date.hebrew.locale._parseInfo=function(_1f){
_1f=_1f||{};
var _20=_2.normalizeLocale(_1f.locale);
var _21=dojox.date.hebrew.locale._getHebrewBundle(_20);
var _22=_1f.formatLength||"short";
var _23=_1f.datePattern||_21["dateFormat-"+_22];
var _24=_1f.timePattern||_21["timeFormat-"+_22];
var _25;
if(_1f.selector=="date"){
_25=_23;
}else{
if(_1f.selector=="time"){
_25=_24;
}else{
_25=(_24===undefined)?_23:_23+" "+_24;
}
}
var _26=[];
var re=_1b(_25,_1.hitch(this,_27,_26,_21,_1f));
return {regexp:re,tokens:_26,bundle:_21};
};
dojox.date.hebrew.locale.parse=function(_28,_29){
_28=_28.replace(/[\u200E\u200F\u202A-\u202E]/g,"");
if(!_29){
_29={};
}
var _2a=dojox.date.hebrew.locale._parseInfo(_29);
var _2b=_2a.tokens,_2c=_2a.bundle;
var re=new RegExp("^"+_2a.regexp+"$");
var _2d=re.exec(_28);
var _2e=_2.normalizeLocale(_29.locale);
if(!_2d){
return null;
}
var _2f,_30;
var _31=[5730,3,23,0,0,0,0];
var _32="";
var _33=0;
var _34=["abbr","wide","narrow"];
var _35=_1.every(_2d,function(v,i){
if(!i){
return true;
}
var _36=_2b[i-1];
var l=_36.length;
switch(_36.charAt(0)){
case "y":
if(_2e.match(/^he(?:-.+)?$/)){
_31[0]=_6.parseYearHebrewLetters(v);
}else{
_31[0]=Number(v);
}
break;
case "M":
if(l>2){
var _37=dojox.date.hebrew.locale.getNames("months",_34[l-3],"format",_2e,new _5(5769,1,1)),_38=dojox.date.hebrew.locale.getNames("months",_34[l-3],"format",_2e,new _5(5768,1,1));
if(!_29.strict){
v=v.replace(".","").toLowerCase();
_37=_1.map(_37,function(s){
return s?s.replace(".","").toLowerCase():s;
});
_38=_1.map(_38,function(s){
return s?s.replace(".","").toLowerCase():s;
});
}
var _39=v;
v=_1.indexOf(_37,_39);
if(v==-1){
v=_1.indexOf(_38,_39);
if(v==-1){
return false;
}
}
_33=l;
}else{
if(_2e.match(/^he(?:-.+)?$/)){
v=_6.parseMonthHebrewLetters(v);
}else{
v--;
}
}
_31[1]=Number(v);
break;
case "D":
_31[1]=0;
case "d":
if(_2e.match(/^he(?:-.+)?$/)){
_31[2]=_6.parseDayHebrewLetters(v);
}else{
_31[2]=Number(v);
}
break;
case "a":
var am=_29.am||_2c["dayPeriods-format-wide-am"],pm=_29.pm||_2c["dayPeriods-format-wide-pm"];
if(!_29.strict){
var _3a=/\./g;
v=v.replace(_3a,"").toLowerCase();
am=am.replace(_3a,"").toLowerCase();
pm=pm.replace(_3a,"").toLowerCase();
}
if(_29.strict&&v!=am&&v!=pm){
return false;
}
_32=(v==pm)?"p":(v==am)?"a":"";
break;
case "K":
if(v==24){
v=0;
}
case "h":
case "H":
case "k":
_31[3]=Number(v);
break;
case "m":
_31[4]=Number(v);
break;
case "s":
_31[5]=Number(v);
break;
case "S":
_31[6]=Number(v);
}
return true;
});
var _3b=+_31[3];
if(_32==="p"&&_3b<12){
_31[3]=_3b+12;
}else{
if(_32==="a"&&_3b==12){
_31[3]=0;
}
}
var _3c=new _5(_31[0],_31[1],_31[2],_31[3],_31[4],_31[5],_31[6]);
if(_33<3&&_31[1]>=5&&!_3c.isLeapYear(_3c.getFullYear())){
_3c.setMonth(_31[1]+1);
}
return _3c;
};
function _1b(_3d,_3e,_3f,_40){
var _41=function(x){
return x;
};
_3e=_3e||_41;
_3f=_3f||_41;
_40=_40||_41;
var _42=_3d.match(/(''|[^'])+/g);
var _43=_3d.charAt(0)=="'";
_1.forEach(_42,function(_44,i){
if(!_44){
_42[i]="";
}else{
_42[i]=(_43?_3f:_3e)(_44);
_43=!_43;
}
});
return _40(_42.join(""));
};
function _27(_45,_46,_47,_48){
_48=_3.escapeString(_48);
var _49=_2.normalizeLocale(_47.locale);
return _48.replace(/([a-z])\1*/ig,function(_4a){
var s;
var c=_4a.charAt(0);
var l=_4a.length;
var p2="",p3="";
if(_47.strict){
if(l>1){
p2="0"+"{"+(l-1)+"}";
}
if(l>2){
p3="0"+"{"+(l-2)+"}";
}
}else{
p2="0?";
p3="0{0,2}";
}
switch(c){
case "y":
s="\\S+";
break;
case "M":
if(_49.match("^he(?:-.+)?$")){
s=(l>2)?"\\S+ ?\\S+":"\\S{1,4}";
}else{
s=(l>2)?"\\S+ ?\\S+":p2+"[1-9]|1[0-2]";
}
break;
case "d":
if(_49.match("^he(?:-.+)?$")){
s="\\S['\"'׳]{1,2}\\S?";
}else{
s="[12]\\d|"+p2+"[1-9]|30";
}
break;
case "E":
if(_49.match("^he(?:-.+)?$")){
s=(l>3)?"\\S+ ?\\S+":"\\S";
}else{
s="\\S+";
}
break;
case "h":
s=p2+"[1-9]|1[0-2]";
break;
case "k":
s=p2+"\\d|1[01]";
break;
case "H":
s=p2+"\\d|1\\d|2[0-3]";
break;
case "K":
s=p2+"[1-9]|1\\d|2[0-4]";
break;
case "m":
case "s":
s=p2+"\\d|[0-5]\\d";
break;
case "S":
s="\\d{"+l+"}";
break;
case "a":
var am=_47.am||_46["dayPeriods-format-wide-am"],pm=_47.pm||_46["dayPeriods-format-wide-pm"];
if(_47.strict){
s=am+"|"+pm;
}else{
s=am+"|"+pm;
if(am!=am.toLowerCase()){
s+="|"+am.toLowerCase();
}
if(pm!=pm.toLowerCase()){
s+="|"+pm.toLowerCase();
}
}
break;
default:
s=".*";
}
if(_45){
_45.push(_4a);
}
return "("+s+")";
}).replace(/[\xa0 ]/g,"[\\s\\xa0]");
};
var _4b=[];
dojox.date.hebrew.locale.addCustomFormats=function(_4c,_4d){
_4b.push({pkg:_4c,name:_4d});
};
dojox.date.hebrew.locale._getHebrewBundle=function(_4e){
var _4f={};
_1.forEach(_4b,function(_50){
var _51=_2.getLocalization(_50.pkg,_50.name,_4e);
_4f=_1.mixin(_4f,_51);
},this);
return _4f;
};
dojox.date.hebrew.locale.addCustomFormats("dojo.cldr","hebrew");
dojox.date.hebrew.locale.getNames=function(_52,_53,_54,_55,_56){
var _57,_58=dojox.date.hebrew.locale._getHebrewBundle(_55),_59=[_52,_54,_53];
if(_54=="standAlone"){
var key=_59.join("-");
_57=_58[key];
if(_57[0]==1){
_57=undefined;
}
}
_59[1]="format";
var _5a=(_57||_58[_59.join("-")]).concat();
if(_52=="months"){
if(_56.isLeapYear(_56.getFullYear())){
_59.push("leap");
_5a[6]=_58[_59.join("-")];
}else{
delete _5a[5];
}
}
return _5a;
};
return dojox.date.hebrew.locale;
});
