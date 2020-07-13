//>>built
define("dojox/dtl/filter/misc",["dojo/_base/lang","dojo/_base/json","../_base"],function(_1,_2,dd){
var _3=_1.getObject("filter.misc",true,dd);
_1.mixin(_3,{filesizeformat:function(_4){
_4=parseFloat(_4);
if(_4<1024){
return (_4==1)?_4+" byte":_4+" bytes";
}else{
if(_4<1024*1024){
return (_4/1024).toFixed(1)+" KB";
}else{
if(_4<1024*1024*1024){
return (_4/1024/1024).toFixed(1)+" MB";
}
}
}
return (_4/1024/1024/1024).toFixed(1)+" GB";
},pluralize:function(_5,_6){
_6=_6||"s";
if(_6.indexOf(",")==-1){
_6=","+_6;
}
var _7=_6.split(",");
if(_7.length>2){
return "";
}
var _8=_7[0];
var _9=_7[1];
if(parseInt(_5,10)!=1){
return _9;
}
return _8;
},_phone2numeric:{a:2,b:2,c:2,d:3,e:3,f:3,g:4,h:4,i:4,j:5,k:5,l:5,m:6,n:6,o:6,p:7,r:7,s:7,t:8,u:8,v:8,w:9,x:9,y:9},phone2numeric:function(_a){
var dm=dd.filter.misc;
_a=_a+"";
var _b="";
for(var i=0;i<_a.length;i++){
var _c=_a.charAt(i).toLowerCase();
(dm._phone2numeric[_c])?_b+=dm._phone2numeric[_c]:_b+=_a.charAt(i);
}
return _b;
},pprint:function(_d){
return _2.toJson(_d);
}});
return _3;
});
