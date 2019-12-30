//>>built
define("dojox/date/islamic/locale",["../..","dojo/_base/lang","dojo/_base/array","dojo/date","dojo/i18n","dojo/regexp","dojo/string","./Date","dojo/i18n!dojo/cldr/nls/islamic"],function(_1,_2,_3,dd,_4,_5,_6,_7,_8){
var _9=_2.getObject("date.islamic.locale",true,_1);
function _a(_b,_c,_d,_e,_f){
return _f.replace(/([a-z])\1*/ig,function(_10){
var s,pad;
var c=_10.charAt(0);
var l=_10.length;
var _11=["abbr","wide","narrow"];
switch(c){
case "G":
s=_c["eraAbbr"][0];
break;
case "y":
s=String(_b.getFullYear());
break;
case "M":
var m=_b.getMonth();
if(l<3){
s=m+1;
pad=true;
}else{
var _12=["months","format",_11[l-3]].join("-");
s=_c[_12][m];
}
break;
case "d":
s=_b.getDate(true);
pad=true;
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
s=dd.getTimezoneName(_b.toGregorian());
if(s){
break;
}
l=4;
case "Z":
var _15=_b.toGregorian().getTimezoneOffset();
var tz=[(_15<=0?"+":"-"),_6.pad(Math.floor(Math.abs(_15)/60),2),_6.pad(Math.abs(_15)%60,2)];
if(l==4){
tz.splice(0,0,"GMT");
tz.splice(3,0,":");
}
s=tz.join("");
break;
default:
throw new Error("dojox.date.islamic.locale.formatPattern: invalid pattern char: "+_f);
}
if(pad){
s=_6.pad(s,l);
}
return s;
});
};
_9.format=function(_16,_17){
_17=_17||{};
var _18=_4.normalizeLocale(_17.locale);
var _19=_17.formatLength||"short";
var _1a=_9._getIslamicBundle(_18);
var str=[];
var _1b=_2.hitch(this,_a,_16,_1a,_18,_17.fullYear);
if(_17.selector=="year"){
var _1c=_16.getFullYear();
return _1c;
}
if(_17.selector!="time"){
var _1d=_17.datePattern||_1a["dateFormat-"+_19];
if(_1d){
str.push(_1e(_1d,_1b));
}
}
if(_17.selector!="date"){
var _1f=_17.timePattern||_1a["timeFormat-"+_19];
if(_1f){
str.push(_1e(_1f,_1b));
}
}
var _20=str.join(" ");
return _20;
};
_9.regexp=function(_21){
return _9._parseInfo(_21).regexp;
};
_9._parseInfo=function(_22){
_22=_22||{};
var _23=_4.normalizeLocale(_22.locale);
var _24=_9._getIslamicBundle(_23);
var _25=_22.formatLength||"short";
var _26=_22.datePattern||_24["dateFormat-"+_25];
var _27=_22.timePattern||_24["timeFormat-"+_25];
var _28;
if(_22.selector=="date"){
_28=_26;
}else{
if(_22.selector=="time"){
_28=_27;
}else{
_28=(typeof (_27)=="undefined")?_26:_26+" "+_27;
}
}
var _29=[];
var re=_1e(_28,_2.hitch(this,_2a,_29,_24,_22));
return {regexp:re,tokens:_29,bundle:_24};
};
_9.parse=function(_2b,_2c){
_2b=_2b.replace(/[\u200E\u200F\u202A\u202E]/g,"");
if(!_2c){
_2c={};
}
var _2d=_9._parseInfo(_2c);
var _2e=_2d.tokens,_8=_2d.bundle;
var _2f=_2d.regexp.replace(/[\u200E\u200F\u202A\u202E]/g,"");
var re=new RegExp("^"+_2f+"$");
var _30=re.exec(_2b);
var _31=_4.normalizeLocale(_2c.locale);
if(!_30){
return null;
}
var _32,_33;
var _34=[1389,0,1,0,0,0,0];
var _35="";
var _36=0;
var _37=["abbr","wide","narrow"];
var _38=_3.every(_30,function(v,i){
if(!i){
return true;
}
var _39=_2e[i-1];
var l=_39.length;
switch(_39.charAt(0)){
case "y":
_34[0]=Number(v);
break;
case "M":
if(l>2){
var _3a=_8["months-format-"+_37[l-3]].concat();
if(!_2c.strict){
v=v.replace(".","").toLowerCase();
_3a=_3.map(_3a,function(s){
return s?s.replace(".","").toLowerCase():s;
});
}
v=_3.indexOf(_3a,v);
if(v==-1){
return false;
}
_36=l;
}else{
v--;
}
_34[1]=Number(v);
break;
case "D":
_34[1]=0;
case "d":
_34[2]=Number(v);
break;
case "a":
var am=_2c.am||_8["dayPeriods-format-wide-am"],pm=_2c.pm||_8["dayPeriods-format-wide-pm"];
if(!_2c.strict){
var _3b=/\./g;
v=v.replace(_3b,"").toLowerCase();
am=am.replace(_3b,"").toLowerCase();
pm=pm.replace(_3b,"").toLowerCase();
}
if(_2c.strict&&v!=am&&v!=pm){
return false;
}
_35=(v==pm)?"p":(v==am)?"a":"";
break;
case "K":
if(v==24){
v=0;
}
case "h":
case "H":
case "k":
_34[3]=Number(v);
break;
case "m":
_34[4]=Number(v);
break;
case "s":
_34[5]=Number(v);
break;
case "S":
_34[6]=Number(v);
}
return true;
});
var _3c=+_34[3];
if(_35==="p"&&_3c<12){
_34[3]=_3c+12;
}else{
if(_35==="a"&&_3c==12){
_34[3]=0;
}
}
var _3d=new _7(_34[0],_34[1],_34[2],_34[3],_34[4],_34[5],_34[6]);
return _3d;
};
function _1e(_3e,_3f,_40,_41){
var _42=function(x){
return x;
};
_3f=_3f||_42;
_40=_40||_42;
_41=_41||_42;
var _43=_3e.match(/(''|[^'])+/g);
var _44=_3e.charAt(0)=="'";
_3.forEach(_43,function(_45,i){
if(!_45){
_43[i]="";
}else{
_43[i]=(_44?_40:_3f)(_45);
_44=!_44;
}
});
return _41(_43.join(""));
};
function _2a(_46,_47,_48,_49){
_49=_5.escapeString(_49);
var _4a=_4.normalizeLocale(_48.locale);
return _49.replace(/([a-z])\1*/ig,function(_4b){
var s;
var c=_4b.charAt(0);
var l=_4b.length;
var p2="",p3="";
if(_48.strict){
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
s="\\d+";
break;
case "M":
s=(l>2)?"\\S+ ?\\S+":p2+"[1-9]|1[0-2]";
break;
case "d":
s="[12]\\d|"+p2+"[1-9]|3[01]";
break;
case "E":
s="\\S+";
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
var am=_48.am||_47["dayPeriods-format-wide-am"],pm=_48.pm||_47["dayPeriods-format-wide-pm"];
if(_48.strict){
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
if(_46){
_46.push(_4b);
}
return "("+s+")";
}).replace(/[\xa0 ]/g,"[\\s\\xa0]");
};
var _4c=[];
_9.addCustomFormats=function(_4d,_4e){
_4c.push({pkg:_4d,name:_4e});
};
_9._getIslamicBundle=function(_4f){
var _50={};
_3.forEach(_4c,function(_51){
var _52=_4.getLocalization(_51.pkg,_51.name,_4f);
_50=_2.mixin(_50,_52);
},this);
return _50;
};
_9.addCustomFormats("dojo.cldr","islamic");
_9.getNames=function(_53,_54,_55,_56,_57){
var _58;
var _59=_9._getIslamicBundle(_56);
var _5a=[_53,_55,_54];
if(_55=="standAlone"){
var key=_5a.join("-");
_58=_59[key];
if(_58[0]==1){
_58=undefined;
}
}
_5a[1]="format";
return (_58||_59[_5a.join("-")]).concat();
};
_9.weekDays=_9.getNames("days","wide","format");
_9.months=_9.getNames("months","wide","format");
return _9;
});
