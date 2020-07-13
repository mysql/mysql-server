/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/date/locale",["../_base/lang","../_base/array","../date","../cldr/supplemental","../i18n","../regexp","../string","../i18n!../cldr/nls/gregorian","module"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a={};
_1.setObject(_9.id.replace(/\//g,"."),_a);
function _b(_c,_d,_e,_f){
return _f.replace(/([a-z])\1*/ig,function(_10){
var s,pad,c=_10.charAt(0),l=_10.length,_11=["abbr","wide","narrow"];
switch(c){
case "G":
s=_d[(l<4)?"eraAbbr":"eraNames"][_c.getFullYear()<0?0:1];
break;
case "y":
s=_c.getFullYear();
switch(l){
case 1:
break;
case 2:
if(!_e.fullYear){
s=String(s);
s=s.substr(s.length-2);
break;
}
default:
pad=true;
}
break;
case "Q":
case "q":
s=Math.ceil((_c.getMonth()+1)/3);
pad=true;
break;
case "M":
case "L":
var m=_c.getMonth();
if(l<3){
s=m+1;
pad=true;
}else{
var _12=["months",c=="L"?"standAlone":"format",_11[l-3]].join("-");
s=_d[_12][m];
}
break;
case "w":
var _13=0;
s=_a._getWeekOfYear(_c,_13);
pad=true;
break;
case "d":
s=_c.getDate();
pad=true;
break;
case "D":
s=_a._getDayOfYear(_c);
pad=true;
break;
case "e":
case "c":
var d=_c.getDay();
if(l<2){
s=(d-_4.getFirstDayOfWeek(_e.locale)+8)%7;
break;
}
case "E":
d=_c.getDay();
if(l<3){
s=d+1;
pad=true;
}else{
var _14=["days",c=="c"?"standAlone":"format",_11[l-3]].join("-");
s=_d[_14][d];
}
break;
case "a":
var _15=_c.getHours()<12?"am":"pm";
s=_e[_15]||_d["dayPeriods-format-wide-"+_15];
break;
case "h":
case "H":
case "K":
case "k":
var h=_c.getHours();
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
s=_c.getMinutes();
pad=true;
break;
case "s":
s=_c.getSeconds();
pad=true;
break;
case "S":
s=Math.round(_c.getMilliseconds()*Math.pow(10,l-3));
pad=true;
break;
case "v":
case "z":
s=_a._getZone(_c,true,_e);
if(s){
break;
}
l=4;
case "Z":
var _16=_a._getZone(_c,false,_e);
var tz=[(_16<=0?"+":"-"),_7.pad(Math.floor(Math.abs(_16)/60),2),_7.pad(Math.abs(_16)%60,2)];
if(l==4){
tz.splice(0,0,"GMT");
tz.splice(3,0,":");
}
s=tz.join("");
break;
default:
throw new Error("dojo.date.locale.format: invalid pattern char: "+_f);
}
if(pad){
s=_7.pad(s,l);
}
return s;
});
};
_a._getZone=function(_17,_18,_19){
if(_18){
return _3.getTimezoneName(_17);
}else{
return _17.getTimezoneOffset();
}
};
_a.format=function(_1a,_1b){
_1b=_1b||{};
var _1c=_5.normalizeLocale(_1b.locale),_1d=_1b.formatLength||"short",_1e=_a._getGregorianBundle(_1c),str=[],_1f=_1.hitch(this,_b,_1a,_1e,_1b);
if(_1b.selector=="year"){
return _20(_1e["dateFormatItem-yyyy"]||"yyyy",_1f);
}
var _21;
if(_1b.selector!="date"){
_21=_1b.timePattern||_1e["timeFormat-"+_1d];
if(_21){
str.push(_20(_21,_1f));
}
}
if(_1b.selector!="time"){
_21=_1b.datePattern||_1e["dateFormat-"+_1d];
if(_21){
str.push(_20(_21,_1f));
}
}
return str.length==1?str[0]:_1e["dateTimeFormat-"+_1d].replace(/\'/g,"").replace(/\{(\d+)\}/g,function(_22,key){
return str[key];
});
};
_a.regexp=function(_23){
return _a._parseInfo(_23).regexp;
};
_a._parseInfo=function(_24){
_24=_24||{};
var _25=_5.normalizeLocale(_24.locale),_26=_a._getGregorianBundle(_25),_27=_24.formatLength||"short",_28=_24.datePattern||_26["dateFormat-"+_27],_29=_24.timePattern||_26["timeFormat-"+_27],_2a;
if(_24.selector=="date"){
_2a=_28;
}else{
if(_24.selector=="time"){
_2a=_29;
}else{
_2a=_26["dateTimeFormat-"+_27].replace(/\{(\d+)\}/g,function(_2b,key){
return [_29,_28][key];
});
}
}
var _2c=[],re=_20(_2a,_1.hitch(this,_2d,_2c,_26,_24));
return {regexp:re,tokens:_2c,bundle:_26};
};
_a.parse=function(_2e,_2f){
var _30=/[\u200E\u200F\u202A\u202E]/g,_31=_a._parseInfo(_2f),_32=_31.tokens,_33=_31.bundle,re=new RegExp("^"+_31.regexp.replace(_30,"")+"$",_31.strict?"":"i"),_34=re.exec(_2e&&_2e.replace(_30,""));
if(!_34){
return null;
}
var _35=["abbr","wide","narrow"],_36=[1970,0,1,0,0,0,0],_37="",_38=_2.every(_34,function(v,i){
if(!i){
return true;
}
var _39=_32[i-1],l=_39.length,c=_39.charAt(0);
switch(c){
case "y":
if(l!=2&&_2f.strict){
_36[0]=v;
}else{
if(v<100){
v=Number(v);
var _3a=""+new Date().getFullYear(),_3b=_3a.substring(0,2)*100,_3c=Math.min(Number(_3a.substring(2,4))+20,99);
_36[0]=(v<_3c)?_3b+v:_3b-100+v;
}else{
if(_2f.strict){
return false;
}
_36[0]=v;
}
}
break;
case "M":
case "L":
if(l>2){
var _3d=_33["months-"+(c=="L"?"standAlone":"format")+"-"+_35[l-3]].concat();
if(!_2f.strict){
v=v.replace(".","").toLowerCase();
_3d=_2.map(_3d,function(s){
return s.replace(".","").toLowerCase();
});
}
v=_2.indexOf(_3d,v);
if(v==-1){
return false;
}
}else{
v--;
}
_36[1]=v;
break;
case "E":
case "e":
case "c":
var _3e=_33["days-"+(c=="c"?"standAlone":"format")+"-"+_35[l-3]].concat();
if(!_2f.strict){
v=v.toLowerCase();
_3e=_2.map(_3e,function(d){
return d.toLowerCase();
});
}
v=_2.indexOf(_3e,v);
if(v==-1){
return false;
}
break;
case "D":
_36[1]=0;
case "d":
_36[2]=v;
break;
case "a":
var am=_2f.am||_33["dayPeriods-format-wide-am"],pm=_2f.pm||_33["dayPeriods-format-wide-pm"];
if(!_2f.strict){
var _3f=/\./g;
v=v.replace(_3f,"").toLowerCase();
am=am.replace(_3f,"").toLowerCase();
pm=pm.replace(_3f,"").toLowerCase();
}
if(_2f.strict&&v!=am&&v!=pm){
return false;
}
_37=(v==pm)?"p":(v==am)?"a":"";
break;
case "K":
if(v==24){
v=0;
}
case "h":
case "H":
case "k":
if(v>23){
return false;
}
_36[3]=v;
break;
case "m":
_36[4]=v;
break;
case "s":
_36[5]=v;
break;
case "S":
_36[6]=v;
}
return true;
});
var _40=+_36[3];
if(_37==="p"&&_40<12){
_36[3]=_40+12;
}else{
if(_37==="a"&&_40==12){
_36[3]=0;
}
}
var _41=new Date(_36[0],_36[1],_36[2],_36[3],_36[4],_36[5],_36[6]);
if(_2f.strict){
_41.setFullYear(_36[0]);
}
var _42=_32.join(""),_43=_42.indexOf("d")!=-1,_44=_42.indexOf("M")!=-1;
if(!_38||(_44&&_41.getMonth()>_36[1])||(_43&&_41.getDate()>_36[2])){
return null;
}
if((_44&&_41.getMonth()<_36[1])||(_43&&_41.getDate()<_36[2])){
_41=_3.add(_41,"hour",1);
}
return _41;
};
function _20(_45,_46,_47,_48){
var _49=function(x){
return x;
};
_46=_46||_49;
_47=_47||_49;
_48=_48||_49;
var _4a=_45.match(/(''|[^'])+/g),_4b=_45.charAt(0)=="'";
_2.forEach(_4a,function(_4c,i){
if(!_4c){
_4a[i]="";
}else{
_4a[i]=(_4b?_47:_46)(_4c.replace(/''/g,"'"));
_4b=!_4b;
}
});
return _48(_4a.join(""));
};
var _4d=["abbr","wide","narrow"];
function _2d(_4e,_4f,_50,_51){
_51=_6.escapeString(_51);
if(!_50.strict){
_51=_51.replace(" a"," ?a");
}
return _51.replace(/([a-z])\1*/ig,function(_52){
var s,c=_52.charAt(0),l=_52.length,p2="",p3="";
if(_50.strict){
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
s="\\d{2,4}";
break;
case "M":
case "L":
if(l>2){
var _53=_4f["months-"+(c=="L"?"standAlone":"format")+"-"+_4d[l-3]].slice(0);
s=_53.join("|");
if(!_50.strict){
s=s.replace(/\./g,"");
s="(?:"+s+")\\.?";
}
}else{
s="1[0-2]|"+p2+"[1-9]";
}
break;
case "D":
s="[12][0-9][0-9]|3[0-5][0-9]|36[0-6]|"+p2+"[1-9][0-9]|"+p3+"[1-9]";
break;
case "d":
s="3[01]|[12]\\d|"+p2+"[1-9]";
break;
case "w":
s="[1-4][0-9]|5[0-3]|"+p2+"[1-9]";
break;
case "E":
case "e":
case "c":
s=".+?";
break;
case "h":
s="1[0-2]|"+p2+"[1-9]";
break;
case "k":
s="1[01]|"+p2+"\\d";
break;
case "H":
s="1\\d|2[0-3]|"+p2+"\\d";
break;
case "K":
s="1\\d|2[0-4]|"+p2+"[1-9]";
break;
case "m":
case "s":
s="[0-5]\\d";
break;
case "S":
s="\\d{"+l+"}";
break;
case "a":
var am=_50.am||_4f["dayPeriods-format-wide-am"],pm=_50.pm||_4f["dayPeriods-format-wide-pm"];
s=am+"|"+pm;
if(!_50.strict){
if(am!=am.toLowerCase()){
s+="|"+am.toLowerCase();
}
if(pm!=pm.toLowerCase()){
s+="|"+pm.toLowerCase();
}
if(s.indexOf(".")!=-1){
s+="|"+s.replace(/\./g,"");
}
}
s=s.replace(/\./g,"\\.");
break;
default:
s=".*";
}
if(_4e){
_4e.push(_52);
}
return "("+s+")";
}).replace(/[\xa0 ]/g,"[\\s\\xa0]");
};
var _54=[];
var _55={};
_a.addCustomFormats=function(_56,_57){
_54.push({pkg:_56,name:_57});
_55={};
};
_a._getGregorianBundle=function(_58){
if(_55[_58]){
return _55[_58];
}
var _59={};
_2.forEach(_54,function(_5a){
var _5b=_5.getLocalization(_5a.pkg,_5a.name,_58);
_59=_1.mixin(_59,_5b);
},this);
return _55[_58]=_59;
};
_a.addCustomFormats(_9.id.replace(/\/date\/locale$/,".cldr"),"gregorian");
_a.getNames=function(_5c,_5d,_5e,_5f){
var _60,_61=_a._getGregorianBundle(_5f),_62=[_5c,_5e,_5d];
if(_5e=="standAlone"){
var key=_62.join("-");
_60=_61[key];
if(_60[0]==1){
_60=undefined;
}
}
_62[1]="format";
return (_60||_61[_62.join("-")]).concat();
};
_a.isWeekend=function(_63,_64){
var _65=_4.getWeekend(_64),day=(_63||new Date()).getDay();
if(_65.end<_65.start){
_65.end+=7;
if(day<_65.start){
day+=7;
}
}
return day>=_65.start&&day<=_65.end;
};
_a._getDayOfYear=function(_66){
return _3.difference(new Date(_66.getFullYear(),0,1,_66.getHours()),_66)+1;
};
_a._getWeekOfYear=function(_67,_68){
if(arguments.length==1){
_68=0;
}
var _69=new Date(_67.getFullYear(),0,1).getDay(),adj=(_69-_68+7)%7,_6a=Math.floor((_a._getDayOfYear(_67)+adj-1)/7);
if(_69==_68){
_6a++;
}
return _6a;
};
return _a;
});
