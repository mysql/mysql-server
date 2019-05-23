//>>built
define("dojox/dtl/filter/strings",["dojo/_base/lang","dojo/_base/array","dojox/string/tokenize","dojox/string/sprintf","../filter/htmlstrings","../_base"],function(_1,_2,_3,_4,_5,dd){
_1.getObject("dojox.dtl.filter.strings",true);
_1.mixin(dd.filter.strings,{_urlquote:function(_6,_7){
if(!_7){
_7="/";
}
return _3(_6,/([^\w-_.])/g,function(_8){
if(_7.indexOf(_8)==-1){
if(_8==" "){
return "+";
}else{
var _9=_8.charCodeAt(0).toString(16).toUpperCase();
while(_9.length<2){
_9="0"+_9;
}
return "%"+_9;
}
}
return _8;
}).join("");
},addslashes:function(_a){
return _a.replace(/\\/g,"\\\\").replace(/"/g,"\\\"").replace(/'/g,"\\'");
},capfirst:function(_b){
_b=""+_b;
return _b.charAt(0).toUpperCase()+_b.substring(1);
},center:function(_c,_d){
_d=_d||_c.length;
_c=_c+"";
var _e=_d-_c.length;
if(_e%2){
_c=_c+" ";
_e-=1;
}
for(var i=0;i<_e;i+=2){
_c=" "+_c+" ";
}
return _c;
},cut:function(_f,arg){
arg=arg+""||"";
_f=_f+"";
return _f.replace(new RegExp(arg,"g"),"");
},_fix_ampersands:/&(?!(\w+|#\d+);)/g,fix_ampersands:function(_10){
return _10.replace(dojox.dtl.filter.strings._fix_ampersands,"&amp;");
},floatformat:function(_11,arg){
arg=parseInt(arg||-1,10);
_11=parseFloat(_11);
var m=_11-_11.toFixed(0);
if(!m&&arg<0){
return _11.toFixed();
}
_11=_11.toFixed(Math.abs(arg));
return (arg<0)?parseFloat(_11)+"":_11;
},iriencode:function(_12){
return dojox.dtl.filter.strings._urlquote(_12,"/#%[]=:;$&()+,!");
},linenumbers:function(_13){
var df=dojox.dtl.filter;
var _14=_13.split("\n");
var _15=[];
var _16=(_14.length+"").length;
for(var i=0,_17;i<_14.length;i++){
_17=_14[i];
_15.push(df.strings.ljust(i+1,_16)+". "+dojox.dtl._base.escape(_17));
}
return _15.join("\n");
},ljust:function(_18,arg){
_18=_18+"";
arg=parseInt(arg,10);
while(_18.length<arg){
_18=_18+" ";
}
return _18;
},lower:function(_19){
return (_19+"").toLowerCase();
},make_list:function(_1a){
var _1b=[];
if(typeof _1a=="number"){
_1a=_1a+"";
}
if(_1a.charAt){
for(var i=0;i<_1a.length;i++){
_1b.push(_1a.charAt(i));
}
return _1b;
}
if(typeof _1a=="object"){
for(var key in _1a){
_1b.push(_1a[key]);
}
return _1b;
}
return [];
},rjust:function(_1c,arg){
_1c=_1c+"";
arg=parseInt(arg,10);
while(_1c.length<arg){
_1c=" "+_1c;
}
return _1c;
},slugify:function(_1d){
_1d=_1d.replace(/[^\w\s-]/g,"").toLowerCase();
return _1d.replace(/[\-\s]+/g,"-");
},_strings:{},stringformat:function(_1e,arg){
arg=""+arg;
var _1f=dojox.dtl.filter.strings._strings;
if(!_1f[arg]){
_1f[arg]=new _4.Formatter("%"+arg);
}
return _1f[arg].format(_1e);
},title:function(_20){
var _21,_22="";
for(var i=0,_23;i<_20.length;i++){
_23=_20.charAt(i);
if(_21==" "||_21=="\n"||_21=="\t"||!_21){
_22+=_23.toUpperCase();
}else{
_22+=_23.toLowerCase();
}
_21=_23;
}
return _22;
},_truncatewords:/[ \n\r\t]/,truncatewords:function(_24,arg){
arg=parseInt(arg,10);
if(!arg){
return _24;
}
for(var i=0,j=_24.length,_25=0,_26,_27;i<_24.length;i++){
_26=_24.charAt(i);
if(dojox.dtl.filter.strings._truncatewords.test(_27)){
if(!dojox.dtl.filter.strings._truncatewords.test(_26)){
++_25;
if(_25==arg){
return _24.substring(0,j+1);
}
}
}else{
if(!dojox.dtl.filter.strings._truncatewords.test(_26)){
j=i;
}
}
_27=_26;
}
return _24;
},_truncate_words:/(&.*?;|<.*?>|(\w[\w\-]*))/g,_truncate_tag:/<(\/)?([^ ]+?)(?: (\/)| .*?)?>/,_truncate_singlets:{br:true,col:true,link:true,base:true,img:true,param:true,area:true,hr:true,input:true},truncatewords_html:function(_28,arg){
arg=parseInt(arg,10);
if(arg<=0){
return "";
}
var _29=dojox.dtl.filter.strings;
var _2a=0;
var _2b=[];
var _2c=_3(_28,_29._truncate_words,function(all,_2d){
if(_2d){
++_2a;
if(_2a<arg){
return _2d;
}else{
if(_2a==arg){
return _2d+" ...";
}
}
}
var tag=all.match(_29._truncate_tag);
if(!tag||_2a>=arg){
return;
}
var _2e=tag[1];
var _2f=tag[2].toLowerCase();
var _30=tag[3];
if(_2e||_29._truncate_singlets[_2f]){
}else{
if(_2e){
var i=_2.indexOf(_2b,_2f);
if(i!=-1){
_2b=_2b.slice(i+1);
}
}else{
_2b.unshift(_2f);
}
}
return all;
}).join("");
_2c=_2c.replace(/\s+$/g,"");
for(var i=0,tag;tag=_2b[i];i++){
_2c+="</"+tag+">";
}
return _2c;
},upper:function(_31){
return _31.toUpperCase();
},urlencode:function(_32){
return dojox.dtl.filter.strings._urlquote(_32);
},_urlize:/^((?:[(>]|&lt;)*)(.*?)((?:[.,)>\n]|&gt;)*)$/,_urlize2:/^\S+@[a-zA-Z0-9._-]+\.[a-zA-Z0-9._-]+$/,urlize:function(_33){
return dojox.dtl.filter.strings.urlizetrunc(_33);
},urlizetrunc:function(_34,arg){
arg=parseInt(arg);
return _3(_34,/(\S+)/g,function(_35){
var _36=dojox.dtl.filter.strings._urlize.exec(_35);
if(!_36){
return _35;
}
var _37=_36[1];
var _38=_36[2];
var _39=_36[3];
var _3a=_38.indexOf("www.")==0;
var _3b=_38.indexOf("@")!=-1;
var _3c=_38.indexOf(":")!=-1;
var _3d=_38.indexOf("http://")==0;
var _3e=_38.indexOf("https://")==0;
var _3f=/[a-zA-Z0-9]/.test(_38.charAt(0));
var _40=_38.substring(_38.length-4);
var _41=_38;
if(arg>3){
_41=_41.substring(0,arg-3)+"...";
}
if(_3a||(!_3b&&!_3d&&_38.length&&_3f&&(_40==".org"||_40==".net"||_40==".com"))){
return "<a href=\"http://"+_38+"\" rel=\"nofollow\">"+_41+"</a>";
}else{
if(_3d||_3e){
return "<a href=\""+_38+"\" rel=\"nofollow\">"+_41+"</a>";
}else{
if(_3b&&!_3a&&!_3c&&dojox.dtl.filter.strings._urlize2.test(_38)){
return "<a href=\"mailto:"+_38+"\">"+_38+"</a>";
}
}
}
return _35;
}).join("");
},wordcount:function(_42){
_42=_1.trim(_42);
if(!_42){
return 0;
}
return _42.split(/\s+/g).length;
},wordwrap:function(_43,arg){
arg=parseInt(arg);
var _44=[];
var _45=_43.split(/\s+/g);
if(_45.length){
var _46=_45.shift();
_44.push(_46);
var pos=_46.length-_46.lastIndexOf("\n")-1;
for(var i=0;i<_45.length;i++){
_46=_45[i];
if(_46.indexOf("\n")!=-1){
var _47=_46.split(/\n/g);
}else{
var _47=[_46];
}
pos+=_47[0].length+1;
if(arg&&pos>arg){
_44.push("\n");
pos=_47[_47.length-1].length;
}else{
_44.push(" ");
if(_47.length>1){
pos=_47[_47.length-1].length;
}
}
_44.push(_46);
}
}
return _44.join("");
}});
return dojox.dtl.filter.strings;
});
