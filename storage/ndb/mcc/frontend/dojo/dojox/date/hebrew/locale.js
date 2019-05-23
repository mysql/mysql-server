//>>built
define("dojox/date/hebrew/locale",["../..","dojo/_base/lang","dojo/_base/array","dojo/date","dojo/i18n","dojo/regexp","dojo/string","./Date","./numerals","dojo/i18n!dojo/cldr/nls/hebrew"],function(_1,_2,_3,dd,_4,_5,_6,_7,_8){
var _9=_2.getObject("date.hebrew.locale",true,_1);
_4.getLocalization("dojo.cldr","hebrew");
function _a(_b,_c,_d,_e,_f){
return _f.replace(/([a-z])\1*/ig,function(_10){
var s,pad;
var c=_10.charAt(0);
var l=_10.length;
var _11=["abbr","wide","narrow"];
switch(c){
case "y":
if(_d.match(/^he(?:-.+)?$/)){
s=_8.getYearHebrewLetters(_b.getFullYear());
}else{
s=String(_b.getFullYear());
}
break;
case "M":
var m=_b.getMonth();
if(l<3){
if(!_b.isLeapYear(_b.getFullYear())&&m>5){
m--;
}
if(_d.match(/^he(?:-.+)?$/)){
s=_8.getMonthHebrewLetters(m);
}else{
s=m+1;
pad=true;
}
}else{
var _12=_9.getNames("months",_11[l-3],"format",_d,_b);
s=_12[m];
}
break;
case "d":
if(_d.match(/^he(?:-.+)?$/)){
s=_b.getDateLocalized(_d);
}else{
s=_b.getDate();
pad=true;
}
break;
case "E":
var d=_b.getDay();
if(l<3){
s=d+1;
pad=true;
}else{
var _13=["days","format",_11[l-3]].join("-");
s=_c[_13][d];
}
break;
case "a":
var _14=(_b.getHours()<12)?"am":"pm";
s=_c["dayPeriods-format-wide-"+_14];
break;
case "h":
case "H":
case "K":
case "k":
var h=_b.getHours();
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
pad=true;
break;
case "m":
s=_b.getMinutes();
pad=true;
break;
case "s":
s=_b.getSeconds();
pad=true;
break;
case "S":
s=Math.round(_b.getMilliseconds()*Math.pow(10,l-3));
pad=true;
break;
case "z":
s="";
break;
default:
throw new Error("dojox.date.hebrew.locale.formatPattern: invalid pattern char: "+_f);
}
if(pad){
s=_6.pad(s,l);
}
return s;
});
};
_9.format=function(_15,_16){
_16=_16||{};
var _17=_4.normalizeLocale(_16.locale);
var _18=_16.formatLength||"short";
var _19=_9._getHebrewBundle(_17);
var str=[];
var _1a=_2.hitch(this,_a,_15,_19,_17,_16.fullYear);
if(_16.selector=="year"){
var _1b=_15.getFullYear();
return _17.match(/^he(?:-.+)?$/)?_8.getYearHebrewLetters(_1b):_1b;
}
if(_16.selector!="time"){
var _1c=_16.datePattern||_19["dateFormat-"+_18];
if(_1c){
str.push(_1d(_1c,_1a));
}
}
if(_16.selector!="date"){
var _1e=_16.timePattern||_19["timeFormat-"+_18];
if(_1e){
str.push(_1d(_1e,_1a));
}
}
var _1f=str.join(" ");
return _1f;
};
_9.regexp=function(_20){
return _9._parseInfo(_20).regexp;
};
_9._parseInfo=function(_21){
_21=_21||{};
var _22=_4.normalizeLocale(_21.locale);
var _23=_9._getHebrewBundle(_22);
var _24=_21.formatLength||"short";
var _25=_21.datePattern||_23["dateFormat-"+_24];
var _26=_21.timePattern||_23["timeFormat-"+_24];
var _27;
if(_21.selector=="date"){
_27=_25;
}else{
if(_21.selector=="time"){
_27=_26;
}else{
_27=(_26===undefined)?_25:_25+" "+_26;
}
}
var _28=[];
var re=_1d(_27,_2.hitch(this,_29,_28,_23,_21));
return {regexp:re,tokens:_28,bundle:_23};
};
_9.parse=function(_2a,_2b){
_2a=_2a.replace(/[\u200E\u200F\u202A-\u202E]/g,"");
if(!_2b){
_2b={};
}
var _2c=_9._parseInfo(_2b);
var _2d=_2c.tokens,_2e=_2c.bundle;
var re=new RegExp("^"+_2c.regexp+"$");
var _2f=re.exec(_2a);
var _30=_4.normalizeLocale(_2b.locale);
if(!_2f){
return null;
}
var _31,_32;
var _33=[5730,3,23,0,0,0,0];
var _34="";
var _35=0;
var _36=["abbr","wide","narrow"];
var _37=_3.every(_2f,function(v,i){
if(!i){
return true;
}
var _38=_2d[i-1];
var l=_38.length;
switch(_38.charAt(0)){
case "y":
if(_30.match(/^he(?:-.+)?$/)){
_33[0]=_8.parseYearHebrewLetters(v);
}else{
_33[0]=Number(v);
}
break;
case "M":
if(l>2){
var _39=_9.getNames("months",_36[l-3],"format",_30,new _7(5769,1,1)),_3a=_9.getNames("months",_36[l-3],"format",_30,new _7(5768,1,1));
if(!_2b.strict){
v=v.replace(".","").toLowerCase();
_39=_3.map(_39,function(s){
return s?s.replace(".","").toLowerCase():s;
});
_3a=_3.map(_3a,function(s){
return s?s.replace(".","").toLowerCase():s;
});
}
var _3b=v;
v=_3.indexOf(_39,_3b);
if(v==-1){
v=_3.indexOf(_3a,_3b);
if(v==-1){
return false;
}
}
_35=l;
}else{
if(_30.match(/^he(?:-.+)?$/)){
v=_8.parseMonthHebrewLetters(v);
}else{
v--;
}
}
_33[1]=Number(v);
break;
case "D":
_33[1]=0;
case "d":
if(_30.match(/^he(?:-.+)?$/)){
_33[2]=_8.parseDayHebrewLetters(v);
}else{
_33[2]=Number(v);
}
break;
case "a":
var am=_2b.am||_2e["dayPeriods-format-wide-am"],pm=_2b.pm||_2e["dayPeriods-format-wide-pm"];
if(!_2b.strict){
var _3c=/\./g;
v=v.replace(_3c,"").toLowerCase();
am=am.replace(_3c,"").toLowerCase();
pm=pm.replace(_3c,"").toLowerCase();
}
if(_2b.strict&&v!=am&&v!=pm){
return false;
}
_34=(v==pm)?"p":(v==am)?"a":"";
break;
case "K":
if(v==24){
v=0;
}
case "h":
case "H":
case "k":
_33[3]=Number(v);
break;
case "m":
_33[4]=Number(v);
break;
case "s":
_33[5]=Number(v);
break;
case "S":
_33[6]=Number(v);
}
return true;
});
var _3d=+_33[3];
if(_34==="p"&&_3d<12){
_33[3]=_3d+12;
}else{
if(_34==="a"&&_3d==12){
_33[3]=0;
}
}
var _3e=new _7(_33[0],_33[1],_33[2],_33[3],_33[4],_33[5],_33[6]);
if(_35<3&&_33[1]>=5&&!_3e.isLeapYear(_3e.getFullYear())){
_3e.setMonth(_33[1]+1);
}
return _3e;
};
function _1d(_3f,_40,_41,_42){
var _43=function(x){
return x;
};
_40=_40||_43;
_41=_41||_43;
_42=_42||_43;
var _44=_3f.match(/(''|[^'])+/g);
var _45=_3f.charAt(0)=="'";
_3.forEach(_44,function(_46,i){
if(!_46){
_44[i]="";
}else{
_44[i]=(_45?_41:_40)(_46);
_45=!_45;
}
});
return _42(_44.join(""));
};
function _29(_47,_48,_49,_4a){
_4a=_5.escapeString(_4a);
var _4b=_4.normalizeLocale(_49.locale);
return _4a.replace(/([a-z])\1*/ig,function(_4c){
var s;
var c=_4c.charAt(0);
var l=_4c.length;
var p2="",p3="";
if(_49.strict){
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
if(_4b.match("^he(?:-.+)?$")){
s=(l>2)?"\\S+ ?\\S+":"\\S{1,4}";
}else{
s=(l>2)?"\\S+ ?\\S+":p2+"[1-9]|1[0-2]";
}
break;
case "d":
if(_4b.match("^he(?:-.+)?$")){
s="\\S['\"'׳]{1,2}\\S?";
}else{
s="[12]\\d|"+p2+"[1-9]|30";
}
break;
case "E":
if(_4b.match("^he(?:-.+)?$")){
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
var am=_49.am||_48["dayPeriods-format-wide-am"],pm=_49.pm||_48["dayPeriods-format-wide-pm"];
if(_49.strict){
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
if(_47){
_47.push(_4c);
}
return "("+s+")";
}).replace(/[\xa0 ]/g,"[\\s\\xa0]");
};
var _4d=[];
_9.addCustomFormats=function(_4e,_4f){
_4d.push({pkg:_4e,name:_4f});
};
_9._getHebrewBundle=function(_50){
var _51={};
_3.forEach(_4d,function(_52){
var _53=_4.getLocalization(_52.pkg,_52.name,_50);
_51=_2.mixin(_51,_53);
},this);
return _51;
};
_9.addCustomFormats("dojo.cldr","hebrew");
_9.getNames=function(_54,_55,_56,_57,_58){
var _59,_5a=_9._getHebrewBundle(_57),_5b=[_54,_56,_55];
if(_56=="standAlone"){
var key=_5b.join("-");
_59=_5a[key];
if(_59[0]==1){
_59=undefined;
}
}
_5b[1]="format";
var _5c=(_59||_5a[_5b.join("-")]).concat();
if(_54=="months"){
if(_58.isLeapYear(_58.getFullYear())){
_5b.push("leap");
_5c[6]=_5a[_5b.join("-")];
}else{
delete _5c[5];
}
}
return _5c;
};
return _9;
});
