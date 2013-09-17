//>>built
define("dojox/date/islamic/locale",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/date","dojo/i18n","dojo/regexp","dojo/string","./Date","dojo/i18n!dojo/cldr/nls/islamic"],function(_1,_2,_3,dd,_4,_5,_6,_7){
_1.getObject("date.islamic.locale",true,dojox);
_1.experimental("dojox.date.islamic.locale");
_1.requireLocalization("dojo.cldr","islamic");
function _8(_9,_a,_b,_c,_d){
return _d.replace(/([a-z])\1*/ig,function(_e){
var s,_f;
var c=_e.charAt(0);
var l=_e.length;
var _10=["abbr","wide","narrow"];
switch(c){
case "G":
s=_a["eraAbbr"][0];
break;
case "y":
s=String(_9.getFullYear());
break;
case "M":
var m=_9.getMonth();
if(l<3){
s=m+1;
_f=true;
}else{
var _11=["months","format",_10[l-3]].join("-");
s=_a[_11][m];
}
break;
case "d":
s=_9.getDate(true);
_f=true;
break;
case "E":
var d=_9.getDay();
if(l<3){
s=d+1;
_f=true;
}else{
var _12=["days","format",_10[l-3]].join("-");
s=_a[_12][d];
}
break;
case "a":
var _13=(_9.getHours()<12)?"am":"pm";
s=_a["dayPeriods-format-wide-"+_13];
break;
case "h":
case "H":
case "K":
case "k":
var h=_9.getHours();
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
_f=true;
break;
case "m":
s=_9.getMinutes();
_f=true;
break;
case "s":
s=_9.getSeconds();
_f=true;
break;
case "S":
s=Math.round(_9.getMilliseconds()*Math.pow(10,l-3));
_f=true;
break;
case "z":
s=dd.getTimezoneName(_9.toGregorian());
if(s){
break;
}
l=4;
case "Z":
var _14=_9.toGregorian().getTimezoneOffset();
var tz=[(_14<=0?"+":"-"),_6.pad(Math.floor(Math.abs(_14)/60),2),_6.pad(Math.abs(_14)%60,2)];
if(l==4){
tz.splice(0,0,"GMT");
tz.splice(3,0,":");
}
s=tz.join("");
break;
default:
throw new Error("dojox.date.islamic.locale.formatPattern: invalid pattern char: "+_d);
}
if(_f){
s=_6.pad(s,l);
}
return s;
});
};
dojox.date.islamic.locale.format=function(_15,_16){
_16=_16||{};
var _17=_4.normalizeLocale(_16.locale);
var _18=_16.formatLength||"short";
var _19=dojox.date.islamic.locale._getIslamicBundle(_17);
var str=[];
var _1a=_1.hitch(this,_8,_15,_19,_17,_16.fullYear);
if(_16.selector=="year"){
var _1b=_15.getFullYear();
return _1b;
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
dojox.date.islamic.locale.regexp=function(_20){
return dojox.date.islamic.locale._parseInfo(_20).regexp;
};
dojox.date.islamic.locale._parseInfo=function(_21){
_21=_21||{};
var _22=_4.normalizeLocale(_21.locale);
var _23=dojox.date.islamic.locale._getIslamicBundle(_22);
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
_27=(typeof (_26)=="undefined")?_25:_25+" "+_26;
}
}
var _28=[];
var re=_1d(_27,_1.hitch(this,_29,_28,_23,_21));
return {regexp:re,tokens:_28,bundle:_23};
};
dojox.date.islamic.locale.parse=function(_2a,_2b){
_2a=_2a.replace(/[\u200E\u200F\u202A\u202E]/g,"");
if(!_2b){
_2b={};
}
var _2c=dojox.date.islamic.locale._parseInfo(_2b);
var _2d=_2c.tokens,_2e=_2c.bundle;
var _2f=_2c.regexp.replace(/[\u200E\u200F\u202A\u202E]/g,"");
var re=new RegExp("^"+_2f+"$");
var _30=re.exec(_2a);
var _31=_4.normalizeLocale(_2b.locale);
if(!_30){
return null;
}
var _32,_33;
var _34=[1389,0,1,0,0,0,0];
var _35="";
var _36=0;
var _37=["abbr","wide","narrow"];
var _38=_1.every(_30,function(v,i){
if(!i){
return true;
}
var _39=_2d[i-1];
var l=_39.length;
switch(_39.charAt(0)){
case "y":
_34[0]=Number(v);
break;
case "M":
if(l>2){
var _3a=_2e["months-format-"+_37[l-3]].concat();
if(!_2b.strict){
v=v.replace(".","").toLowerCase();
_3a=_1.map(_3a,function(s){
return s?s.replace(".","").toLowerCase():s;
});
}
v=_1.indexOf(_3a,v);
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
var am=_2b.am||_2e["dayPeriods-format-wide-am"],pm=_2b.pm||_2e["dayPeriods-format-wide-pm"];
if(!_2b.strict){
var _3b=/\./g;
v=v.replace(_3b,"").toLowerCase();
am=am.replace(_3b,"").toLowerCase();
pm=pm.replace(_3b,"").toLowerCase();
}
if(_2b.strict&&v!=am&&v!=pm){
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
function _1d(_3e,_3f,_40,_41){
var _42=function(x){
return x;
};
_3f=_3f||_42;
_40=_40||_42;
_41=_41||_42;
var _43=_3e.match(/(''|[^'])+/g);
var _44=_3e.charAt(0)=="'";
_1.forEach(_43,function(_45,i){
if(!_45){
_43[i]="";
}else{
_43[i]=(_44?_40:_3f)(_45);
_44=!_44;
}
});
return _41(_43.join(""));
};
function _29(_46,_47,_48,_49){
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
dojox.date.islamic.locale.addCustomFormats=function(_4d,_4e){
_4c.push({pkg:_4d,name:_4e});
};
dojox.date.islamic.locale._getIslamicBundle=function(_4f){
var _50={};
_1.forEach(_4c,function(_51){
var _52=_4.getLocalization(_51.pkg,_51.name,_4f);
_50=_1.mixin(_50,_52);
},this);
return _50;
};
dojox.date.islamic.locale.addCustomFormats("dojo.cldr","islamic");
dojox.date.islamic.locale.getNames=function(_53,_54,_55,_56,_57){
var _58;
var _59=dojox.date.islamic.locale._getIslamicBundle(_56);
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
dojox.date.islamic.locale.weekDays=dojox.date.islamic.locale.getNames("days","wide","format");
dojox.date.islamic.locale.months=dojox.date.islamic.locale.getNames("months","wide","format");
return dojox.date.islamic.locale;
});
