//>>built
define("dojox/dtl/filter/strings",["dojo/_base/lang","dojo/_base/array","dojox/string/tokenize","dojox/string/sprintf","../filter/htmlstrings","../_base"],function(_1,_2,_3,_4,_5,dd){
var _6=_1.getObject("filter.strings",true,dd);
_1.mixin(_6,{_urlquote:function(_7,_8){
if(!_8){
_8="/";
}
return _3(_7,/([^\w-_.])/g,function(_9){
if(_8.indexOf(_9)==-1){
if(_9==" "){
return "+";
}else{
var _a=_9.charCodeAt(0).toString(16).toUpperCase();
while(_a.length<2){
_a="0"+_a;
}
return "%"+_a;
}
}
return _9;
}).join("");
},addslashes:function(_b){
return _b.replace(/\\/g,"\\\\").replace(/"/g,"\\\"").replace(/'/g,"\\'");
},capfirst:function(_c){
_c=""+_c;
return _c.charAt(0).toUpperCase()+_c.substring(1);
},center:function(_d,_e){
_e=_e||_d.length;
_d=_d+"";
var _f=_e-_d.length;
if(_f%2){
_d=_d+" ";
_f-=1;
}
for(var i=0;i<_f;i+=2){
_d=" "+_d+" ";
}
return _d;
},cut:function(_10,arg){
arg=arg+""||"";
_10=_10+"";
return _10.replace(new RegExp(arg,"g"),"");
},_fix_ampersands:/&(?!(\w+|#\d+);)/g,fix_ampersands:function(_11){
return _11.replace(_6._fix_ampersands,"&amp;");
},floatformat:function(_12,arg){
arg=parseInt(arg||-1,10);
_12=parseFloat(_12);
var m=_12-_12.toFixed(0);
if(!m&&arg<0){
return _12.toFixed();
}
_12=_12.toFixed(Math.abs(arg));
return (arg<0)?parseFloat(_12)+"":_12;
},iriencode:function(_13){
return _6._urlquote(_13,"/#%[]=:;$&()+,!");
},linenumbers:function(_14){
var df=dojox.dtl.filter;
var _15=_14.split("\n");
var _16=[];
var _17=(_15.length+"").length;
for(var i=0,_18;i<_15.length;i++){
_18=_15[i];
_16.push(df.strings.ljust(i+1,_17)+". "+dojox.dtl._base.escape(_18));
}
return _16.join("\n");
},ljust:function(_19,arg){
_19=_19+"";
arg=parseInt(arg,10);
while(_19.length<arg){
_19=_19+" ";
}
return _19;
},lower:function(_1a){
return (_1a+"").toLowerCase();
},make_list:function(_1b){
var _1c=[];
if(typeof _1b=="number"){
_1b=_1b+"";
}
if(_1b.charAt){
for(var i=0;i<_1b.length;i++){
_1c.push(_1b.charAt(i));
}
return _1c;
}
if(typeof _1b=="object"){
for(var key in _1b){
_1c.push(_1b[key]);
}
return _1c;
}
return [];
},rjust:function(_1d,arg){
_1d=_1d+"";
arg=parseInt(arg,10);
while(_1d.length<arg){
_1d=" "+_1d;
}
return _1d;
},slugify:function(_1e){
_1e=_1e.replace(/[^\w\s-]/g,"").toLowerCase();
return _1e.replace(/[\-\s]+/g,"-");
},_strings:{},stringformat:function(_1f,arg){
arg=""+arg;
var _20=_6._strings;
if(!_20[arg]){
_20[arg]=new _4.Formatter("%"+arg);
}
return _20[arg].format(_1f);
},title:function(_21){
var _22,_23="";
for(var i=0,_24;i<_21.length;i++){
_24=_21.charAt(i);
if(_22==" "||_22=="\n"||_22=="\t"||!_22){
_23+=_24.toUpperCase();
}else{
_23+=_24.toLowerCase();
}
_22=_24;
}
return _23;
},_truncatewords:/[ \n\r\t]/,truncatewords:function(_25,arg){
arg=parseInt(arg,10);
if(!arg){
return _25;
}
for(var i=0,j=_25.length,_26=0,_27,_28;i<_25.length;i++){
_27=_25.charAt(i);
if(_6._truncatewords.test(_28)){
if(!_6._truncatewords.test(_27)){
++_26;
if(_26==arg){
return _25.substring(0,j+1)+" ...";
}
}
}else{
if(!_6._truncatewords.test(_27)){
j=i;
}
}
_28=_27;
}
return _25;
},_truncate_words:/(&.*?;|<.*?>|(\w[\w\-]*))/g,_truncate_tag:/<(\/)?([^ ]+?)(?: (\/)| .*?)?>/,_truncate_singlets:{br:true,col:true,link:true,base:true,img:true,param:true,area:true,hr:true,input:true},truncatewords_html:function(_29,arg){
arg=parseInt(arg,10);
if(arg<=0){
return "";
}
var _2a=0;
var _2b=[];
var _2c=_3(_29,_6._truncate_words,function(all,_2d){
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
var tag=all.match(_6._truncate_tag);
if(!tag||_2a>=arg){
return;
}
var _2e=tag[1];
var _2f=tag[2].toLowerCase();
var _30=tag[3];
if(_2e||_6._truncate_singlets[_2f]){
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
return _6._urlquote(_32);
},_urlize:/^((?:[(>]|&lt;)*)(.*?)((?:[.,)>\n]|&gt;)*)$/,_urlize2:/^\S+@[a-zA-Z0-9._-]+\.[a-zA-Z0-9._-]+$/,urlize:function(_33){
return _6.urlizetrunc(_33);
},urlizetrunc:function(_34,arg){
arg=parseInt(arg);
return _3(_34,/(\S+)/g,function(_35){
var _36=_6._urlize.exec(_35);
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
if(_3b&&!_3a&&!_3c&&_6._urlize2.test(_38)){
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
return _6;
});
