//>>built
define("dojox/dtl/filter/misc",["dojo/_base/lang","dojo/_base/json","../_base"],function(_1,_2,dd){
_1.getObject("dojox.dtl.filter.misc",true);
_1.mixin(dd.filter.misc,{filesizeformat:function(_3){
_3=parseFloat(_3);
if(_3<1024){
return (_3==1)?_3+" byte":_3+" bytes";
}else{
if(_3<1024*1024){
return (_3/1024).toFixed(1)+" KB";
}else{
if(_3<1024*1024*1024){
return (_3/1024/1024).toFixed(1)+" MB";
}
}
}
return (_3/1024/1024/1024).toFixed(1)+" GB";
},pluralize:function(_4,_5){
_5=_5||"s";
if(_5.indexOf(",")==-1){
_5=","+_5;
}
var _6=_5.split(",");
if(_6.length>2){
return "";
}
var _7=_6[0];
var _8=_6[1];
if(parseInt(_4,10)!=1){
return _8;
}
return _7;
},_phone2numeric:{a:2,b:2,c:2,d:3,e:3,f:3,g:4,h:4,i:4,j:5,k:5,l:5,m:6,n:6,o:6,p:7,r:7,s:7,t:8,u:8,v:8,w:9,x:9,y:9},phone2numeric:function(_9){
var dm=dd.filter.misc;
_9=_9+"";
var _a="";
for(var i=0;i<_9.length;i++){
var _b=_9.charAt(i).toLowerCase();
(dm._phone2numeric[_b])?_a+=dm._phone2numeric[_b]:_a+=_9.charAt(i);
}
return _a;
},pprint:function(_c){
return _2.toJson(_c);
}});
return dojox.dtl.filter.misc;
});
