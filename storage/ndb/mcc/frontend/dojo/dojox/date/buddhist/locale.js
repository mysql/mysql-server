//>>built
define("dojox/date/buddhist/locale",["../..","dojo/_base/lang","dojo/_base/array","dojo/date","dojo/i18n","dojo/regexp","dojo/string","./Date","dojo/i18n!dojo/cldr/nls/buddhist"],function(_1,_2,_3,dd,_4,_5,_6,_7){
var _8=_2.getObject("date.buddhist.locale",true,_1);
function _9(_a,_b,_c,_d,_e){
return _e.replace(/([a-z])\1*/ig,function(_f){
var s,pad;
var c=_f.charAt(0);
var l=_f.length;
var _10=["abbr","wide","narrow"];
switch(c){
case "G":
s=_b["eraAbbr"][0];
break;
case "y":
s=String(_a.getFullYear());
break;
case "M":
var m=_a.getMonth();
if(l<3){
s=m+1;
pad=true;
}else{
var _11=["months","format",_10[l-3]].join("-");
s=_b[_11][m];
}
break;
case "d":
s=_a.getDate(true);
pad=true;
break;
case "E":
var d=_a.getDay();
if(l<3){
s=d+1;
pad=true;
}else{
var _12=["days","format",_10[l-3]].join("-");
s=_b[_12][d];
}
break;
case "a":
var _13=(_a.getHours()<12)?"am":"pm";
s=_b["dayPeriods-format-wide-"+_13];
break;
case "h":
case "H":
case "K":
case "k":
var h=_a.getHours();
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
s=_a.getMinutes();
pad=true;
break;
case "s":
s=_a.getSeconds();
pad=true;
break;
case "S":
s=Math.round(_a.getMilliseconds()*Math.pow(10,l-3));
pad=true;
break;
case "z":
s=dd.getTimezoneName(_a.toGregorian());
if(s){
break;
}
l=4;
case "Z":
var _14=_a.toGregorian().getTimezoneOffset();
var tz=[(_14<=0?"+":"-"),_6.pad(Math.floor(Math.abs(_14)/60),2),_6.pad(Math.abs(_14)%60,2)];
if(l==4){
tz.splice(0,0,"GMT");
tz.splice(3,0,":");
}
s=tz.join("");
break;
default:
throw new Error("dojox.date.buddhist.locale.formatPattern: invalid pattern char: "+_e);
}
if(pad){
s=_6.pad(s,l);
}
return s;
});
};
_8.format=function(_15,_16){
_16=_16||{};
var _17=_4.normalizeLocale(_16.locale);
var _18=_16.formatLength||"short";
var _19=_8._getBuddhistBundle(_17);
var str=[];
var _1a=_2.hitch(this,_9,_15,_19,_17,_16.fullYear);
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
_8.regexp=function(_20){
return _8._parseInfo(_20).regexp;
};
_8._parseInfo=function(_21){
_21=_21||{};
var _22=_4.normalizeLocale(_21.locale);
var _23=_8._getBuddhistBundle(_22);
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
var re=_1d(_27,_2.hitch(this,_29,_28,_23,_21));
return {regexp:re,tokens:_28,bundle:_23};
};
_8.parse=function(_2a,_2b){
_2a=_2a.replace(/[\u200E\u200F\u202A-\u202E]/g,"");
if(!_2b){
_2b={};
}
var _2c=_8._parseInfo(_2b);
var _2d=_2c.tokens,_2e=_2c.bundle;
var re=new RegExp("^"+_2c.regexp+"$");
var _2f=re.exec(_2a);
var _30=_4.normalizeLocale(_2b.locale);
if(!_2f){
return null;
}
var _31,_32;
var _33=[2513,0,1,0,0,0,0];
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
_33[0]=Number(v);
break;
case "M":
if(l>2){
var _39=_2e["months-format-"+_36[l-3]].concat();
if(!_2b.strict){
v=v.replace(".","").toLowerCase();
_39=_3.map(_39,function(s){
return s?s.replace(".","").toLowerCase():s;
});
}
v=_3.indexOf(_39,v);
if(v==-1){
return false;
}
_35=l;
}else{
v--;
}
_33[1]=Number(v);
break;
case "D":
_33[1]=0;
case "d":
_33[2]=Number(v);
break;
case "a":
var am=_2b.am||_2e["dayPeriods-format-wide-am"],pm=_2b.pm||_2e["dayPeriods-format-wide-pm"];
if(!_2b.strict){
var _3a=/\./g;
v=v.replace(_3a,"").toLowerCase();
am=am.replace(_3a,"").toLowerCase();
pm=pm.replace(_3a,"").toLowerCase();
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
var _3b=+_33[3];
if(_34==="p"&&_3b<12){
_33[3]=_3b+12;
}else{
if(_34==="a"&&_3b==12){
_33[3]=0;
}
}
var _3c=new _7(_33[0],_33[1],_33[2],_33[3],_33[4],_33[5],_33[6]);
return _3c;
};
function _1d(_3d,_3e,_3f,_40){
var _41=function(x){
return x;
};
_3e=_3e||_41;
_3f=_3f||_41;
_40=_40||_41;
var _42=_3d.match(/(''|[^'])+/g);
var _43=_3d.charAt(0)=="'";
_3.forEach(_42,function(_44,i){
if(!_44){
_42[i]="";
}else{
_42[i]=(_43?_3f:_3e)(_44);
_43=!_43;
}
});
return _40(_42.join(""));
};
function _29(_45,_46,_47,_48){
_48=_5.escapeString(_48);
var _49=_4.normalizeLocale(_47.locale);
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
s="\\d+";
break;
case "M":
s=(l>2)?"\\S+":p2+"[1-9]|1[0-2]";
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
_8.addCustomFormats=function(_4c,_4d){
_4b.push({pkg:_4c,name:_4d});
};
_8._getBuddhistBundle=function(_4e){
var _4f={};
_3.forEach(_4b,function(_50){
var _51=_4.getLocalization(_50.pkg,_50.name,_4e);
_4f=_2.mixin(_4f,_51);
},this);
return _4f;
};
_8.addCustomFormats("dojo.cldr","buddhist");
_8.getNames=function(_52,_53,_54,_55,_56){
var _57;
var _58=_8._getBuddhistBundle(_55);
var _59=[_52,_54,_53];
if(_54=="standAlone"){
var key=_59.join("-");
_57=_58[key];
if(_57[0]==1){
_57=undefined;
}
}
_59[1]="format";
return (_57||_58[_59.join("-")]).concat();
};
return _8;
});
