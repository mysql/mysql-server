//>>built
define("dojox/io/httpParse",["dojo/_base/kernel"],function(_1){
_1.getObject("io.httpParse",true,dojox);
dojox.io.httpParse=function(_2,_3,_4){
var _5=[];
var _6=_2.length;
do{
var _7={};
var _8=_2.match(/(\n*[^\n]+)/);
if(!_8){
return null;
}
_2=_2.substring(_8[0].length+1);
_8=_8[1];
var _9=_2.match(/([^\n]+\n)*/)[0];
_2=_2.substring(_9.length);
var _a=_2.substring(0,1);
_2=_2.substring(1);
_9=(_3||"")+_9;
var _b=_9;
_9=_9.match(/[^:\n]+:[^\n]+\n/g);
for(var j=0;j<_9.length;j++){
var _c=_9[j].indexOf(":");
_7[_9[j].substring(0,_c)]=_9[j].substring(_c+1).replace(/(^[ \r\n]*)|([ \r\n]*)$/g,"");
}
_8=_8.split(" ");
var _d={status:parseInt(_8[1],10),statusText:_8[2],readyState:3,getAllResponseHeaders:function(){
return _b;
},getResponseHeader:function(_e){
return _7[_e];
}};
var _f=_7["Content-Length"];
var _10;
if(_f){
if(_f<=_2.length){
_10=_2.substring(0,_f);
}else{
return _5;
}
}else{
if((_10=_2.match(/(.*)HTTP\/\d\.\d \d\d\d[\w\s]*\n/))){
_10=_10[0];
}else{
if(!_4||_a=="\n"){
_10=_2;
}else{
return _5;
}
}
}
_5.push(_d);
_2=_2.substring(_10.length);
_d.responseText=_10;
_d.readyState=4;
_d._lastIndex=_6-_2.length;
}while(_2);
return _5;
};
return dojox.io.httpParse;
});
