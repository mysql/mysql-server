//>>built
define("dojox/help/console",["dojo","dijit","dojox","dojo/require!dojox/help/_base"],function(_1,_2,_3){
_1.provide("dojox.help.console");
_1.require("dojox.help._base");
_1.mixin(_3.help,{_plainText:function(_4){
return _4.replace(/(<[^>]*>|&[^;]{2,6};)/g,"");
},_displayLocated:function(_5){
var _6={};
_1.forEach(_5,function(_7){
_6[_7[0]]=_1.isMoz?{toString:function(){
return "Click to view";
},item:_7[1]}:_7[1];
});
},_displayHelp:function(_8,_9){
if(_8){
var _a="Help for: "+_9.name;
var _b="";
for(var i=0;i<_a.length;i++){
_b+="=";
}
}else{
if(!_9){
}else{
var _c=false;
for(var _d in _9){
var _e=_9[_d];
if(_d=="returns"&&_9.type!="Function"&&_9.type!="Constructor"){
continue;
}
if(_e&&(!_1.isArray(_e)||_e.length)){
_c=true;
_e=_1.isString(_e)?_3.help._plainText(_e):_e;
if(_d=="returns"){
var _f=_1.map(_e.types||[],"return item.title;").join("|");
if(_e.summary){
if(_f){
_f+=": ";
}
_f+=_3.help._plainText(_e.summary);
}
}else{
if(_d=="parameters"){
for(var j=0,_10;_10=_e[j];j++){
var _11=_1.map(_10.types,"return item.title").join("|");
var _12="";
if(_10.optional){
_12+="Optional. ";
}
if(_10.repating){
_12+="Repeating. ";
}
_12+=_3.help._plainText(_10.summary);
if(_12){
_12="  - "+_12;
for(var k=0;k<_10.name.length;k++){
_12=" "+_12;
}
}
}
}else{
}
}
}
}
if(!_c){
}
}
}
}});
_3.help.init();
});
