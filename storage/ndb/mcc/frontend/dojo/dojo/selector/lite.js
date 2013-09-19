/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/selector/lite",["../has","../_base/kernel"],function(_1,_2){
"use strict";
var _3=document.createElement("div");
var _4=_3.matchesSelector||_3.webkitMatchesSelector||_3.mozMatchesSelector||_3.msMatchesSelector||_3.oMatchesSelector;
var _5=_3.querySelectorAll;
_1.add("dom-matches-selector",!!_4);
_1.add("dom-qsa",!!_5);
var _6=function(_7,_8){
if(_9&&_7.indexOf(",")>-1){
return _9(_7,_8);
}
var _a=(_5?/^([\w]*)#([\w\-]+$)|^(\.)([\w\-\*]+$)|^(\w+$)/:/^([\w]*)#([\w\-]+)(?:\s+(.*))?$|(?:^|(>|.+\s+))([\w\-\*]+)(\S*$)/).exec(_7);
_8=_8||document;
if(_a){
if(_a[2]){
var _b=_2.byId?_2.byId(_a[2]):document.getElementById(_a[2]);
if(!_b||(_a[1]&&_a[1]!=_b.tagName.toLowerCase())){
return [];
}
if(_8!=document){
var _c=_b;
while(_c!=_8){
_c=_c.parentNode;
if(!_c){
return [];
}
}
}
return _a[3]?_6(_a[3],_b):[_b];
}
if(_a[3]&&_8.getElementsByClassName){
return _8.getElementsByClassName(_a[4]);
}
var _b;
if(_a[5]){
_b=_8.getElementsByTagName(_a[5]);
if(_a[4]||_a[6]){
_7=(_a[4]||"")+_a[6];
}else{
return _b;
}
}
}
if(_5){
if(_8.nodeType===1&&_8.nodeName.toLowerCase()!=="object"){
return _d(_8,_7,_8.querySelectorAll);
}else{
return _8.querySelectorAll(_7);
}
}else{
if(!_b){
_b=_8.getElementsByTagName("*");
}
}
var _e=[];
for(var i=0,l=_b.length;i<l;i++){
var _f=_b[i];
if(_f.nodeType==1&&_10(_f,_7,_8)){
_e.push(_f);
}
}
return _e;
};
var _d=function(_11,_12,_13){
var _14=_11,old=_11.getAttribute("id"),nid=old||"__dojo__",_15=_11.parentNode,_16=/^\s*[+~]/.test(_12);
if(_16&&!_15){
return [];
}
if(!old){
_11.setAttribute("id",nid);
}else{
nid=nid.replace(/'/g,"\\$&");
}
if(_16&&_15){
_11=_11.parentNode;
}
try{
return _13.call(_11,"[id='"+nid+"'] "+_12);
}
finally{
if(!old){
_14.removeAttribute("id");
}
}
};
if(!_1("dom-matches-selector")){
var _10=(function(){
var _17=_3.tagName=="div"?"toLowerCase":"toUpperCase";
function tag(_18){
_18=_18[_17]();
return function(_19){
return _19.tagName==_18;
};
};
function _1a(_1b){
var _1c=" "+_1b+" ";
return function(_1d){
return _1d.className.indexOf(_1b)>-1&&(" "+_1d.className+" ").indexOf(_1c)>-1;
};
};
var _1e={"^=":function(_1f,_20){
return _1f.indexOf(_20)==0;
},"*=":function(_21,_22){
return _21.indexOf(_22)>-1;
},"$=":function(_23,_24){
return _23.substring(_23.length-_24.length,_23.length)==_24;
},"~=":function(_25,_26){
return (" "+_25+" ").indexOf(" "+_26+" ")>-1;
},"|=":function(_27,_28){
return (_27+"-").indexOf(_28+"-")==0;
},"=":function(_29,_2a){
return _29==_2a;
},"":function(_2b,_2c){
return true;
}};
function _2d(_2e,_2f,_30){
if(_2f.match(/['"]/)){
_2f=eval(_2f);
}
var _31=_1e[_30||""];
return function(_32){
var _33=_32.getAttribute(_2e);
return _33&&_31(_33,_2f);
};
};
function _34(_35){
return function(_36,_37){
while((_36=_36.parentNode)!=_37){
if(_35(_36,_37)){
return true;
}
}
};
};
function _38(_39){
return function(_3a,_3b){
_3a=_3a.parentNode;
return _39?_3a!=_3b&&_39(_3a,_3b):_3a==_3b;
};
};
var _3c={};
function and(_3d,_3e){
return _3d?function(_3f,_40){
return _3e(_3f)&&_3d(_3f,_40);
}:_3e;
};
return function(_41,_42,_43){
var _44=_3c[_42];
if(!_44){
if(_42.replace(/(?:\s*([> ])\s*)|(\.)?([\w-]+)|\[([\w-]+)\s*(.?=)?\s*([^\]]*)\]/g,function(t,_45,_46,_47,_48,_49,_4a){
if(_47){
if(_46=="."){
_44=and(_44,_1a(_47));
}else{
_44=and(_44,tag(_47));
}
}else{
if(_45){
_44=(_45==" "?_34:_38)(_44);
}else{
if(_48){
_44=and(_44,_2d(_48,_4a,_49));
}
}
}
return "";
})){
throw new Error("Syntax error in query");
}
if(!_44){
return true;
}
_3c[_42]=_44;
}
return _44(_41,_43);
};
})();
}
if(!_1("dom-qsa")){
var _9=function(_4b,_4c){
_4b=_4b.split(/\s*,\s*/);
var _4d=[];
for(var i=0;i<_4b.length;i++){
var _4e=_6(_4b[i],_4c);
for(var j=0,l=_4e.length;j<l;j++){
var _4f=_4e[j];
_4d[_4f.sourceIndex]=_4f;
}
}
var _50=[];
for(i in _4d){
_50.push(_4d[i]);
}
return _50;
};
}
_6.match=_4?function(_51,_52,_53){
if(_53){
return _d(_53,_52,function(_54){
return _4.call(_51,_54);
});
}
return _4.call(_51,_52);
}:_10;
return _6;
});
