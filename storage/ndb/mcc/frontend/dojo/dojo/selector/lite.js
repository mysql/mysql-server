/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/selector/lite",["../has","../_base/kernel"],function(_1,_2){
"use strict";
var _3=document.createElement("div");
var _4=_3.matches||_3.webkitMatchesSelector||_3.mozMatchesSelector||_3.msMatchesSelector||_3.oMatchesSelector;
var _5=_3.querySelectorAll;
var _6=/([^\s,](?:"(?:\\.|[^"])+"|'(?:\\.|[^'])+'|[^,])*)/g;
_1.add("dom-matches-selector",!!_4);
_1.add("dom-qsa",!!_5);
var _7=function(_8,_9){
if(_a&&_8.indexOf(",")>-1){
return _a(_8,_9);
}
var _b=_9?_9.ownerDocument||_9:_2.doc||document,_c=(_5?/^([\w]*)#([\w\-]+$)|^(\.)([\w\-\*]+$)|^(\w+$)/:/^([\w]*)#([\w\-]+)(?:\s+(.*))?$|(?:^|(>|.+\s+))([\w\-\*]+)(\S*$)/).exec(_8);
_9=_9||_b;
if(_c){
var _d=_1("ie")===8&&_1("quirks")?_9.nodeType===_b.nodeType:_9.parentNode!==null&&_9.nodeType!==9&&_9.parentNode===_b;
if(_c[2]&&_d){
var _e=_2.byId?_2.byId(_c[2],_b):_b.getElementById(_c[2]);
if(!_e||(_c[1]&&_c[1]!=_e.tagName.toLowerCase())){
return [];
}
if(_9!=_b){
var _f=_e;
while(_f!=_9){
_f=_f.parentNode;
if(!_f){
return [];
}
}
}
return _c[3]?_7(_c[3],_e):[_e];
}
if(_c[3]&&_9.getElementsByClassName){
return _9.getElementsByClassName(_c[4]);
}
var _e;
if(_c[5]){
_e=_9.getElementsByTagName(_c[5]);
if(_c[4]||_c[6]){
_8=(_c[4]||"")+_c[6];
}else{
return _e;
}
}
}
if(_5){
if(_9.nodeType===1&&_9.nodeName.toLowerCase()!=="object"){
return _10(_9,_8,_9.querySelectorAll);
}else{
return _9.querySelectorAll(_8);
}
}else{
if(!_e){
_e=_9.getElementsByTagName("*");
}
}
var _11=[];
for(var i=0,l=_e.length;i<l;i++){
var _12=_e[i];
if(_12.nodeType==1&&_13(_12,_8,_9)){
_11.push(_12);
}
}
return _11;
};
var _10=function(_14,_15,_16){
var _17=_14,old=_14.getAttribute("id"),nid=old||"__dojo__",_18=_14.parentNode,_19=/^\s*[+~]/.test(_15);
if(_19&&!_18){
return [];
}
if(!old){
_14.setAttribute("id",nid);
}else{
nid=nid.replace(/'/g,"\\$&");
}
if(_19&&_18){
_14=_14.parentNode;
}
var _1a=_15.match(_6);
for(var i=0;i<_1a.length;i++){
_1a[i]="[id='"+nid+"'] "+_1a[i];
}
_15=_1a.join(",");
try{
return _16.call(_14,_15);
}
finally{
if(!old){
_17.removeAttribute("id");
}
}
};
if(!_1("dom-matches-selector")){
var _13=(function(){
var _1b=_3.tagName=="div"?"toLowerCase":"toUpperCase";
var _1c={"":function(_1d){
_1d=_1d[_1b]();
return function(_1e){
return _1e.tagName==_1d;
};
},".":function(_1f){
var _20=" "+_1f+" ";
return function(_21){
return _21.className.indexOf(_1f)>-1&&(" "+_21.className+" ").indexOf(_20)>-1;
};
},"#":function(id){
return function(_22){
return _22.id==id;
};
}};
var _23={"^=":function(_24,_25){
return _24.indexOf(_25)==0;
},"*=":function(_26,_27){
return _26.indexOf(_27)>-1;
},"$=":function(_28,_29){
return _28.substring(_28.length-_29.length,_28.length)==_29;
},"~=":function(_2a,_2b){
return (" "+_2a+" ").indexOf(" "+_2b+" ")>-1;
},"|=":function(_2c,_2d){
return (_2c+"-").indexOf(_2d+"-")==0;
},"=":function(_2e,_2f){
return _2e==_2f;
},"":function(_30,_31){
return true;
}};
function _32(_33,_34,_35){
var _36=_34.charAt(0);
if(_36=="\""||_36=="'"){
_34=_34.slice(1,-1);
}
_34=_34.replace(/\\/g,"");
var _37=_23[_35||""];
return function(_38){
var _39=_38.getAttribute(_33);
return _39&&_37(_39,_34);
};
};
function _3a(_3b){
return function(_3c,_3d){
while((_3c=_3c.parentNode)!=_3d){
if(_3b(_3c,_3d)){
return true;
}
}
};
};
function _3e(_3f){
return function(_40,_41){
_40=_40.parentNode;
return _3f?_40!=_41&&_3f(_40,_41):_40==_41;
};
};
var _42={};
function and(_43,_44){
return _43?function(_45,_46){
return _44(_45)&&_43(_45,_46);
}:_44;
};
return function(_47,_48,_49){
var _4a=_42[_48];
if(!_4a){
if(_48.replace(/(?:\s*([> ])\s*)|(#|\.)?((?:\\.|[\w-])+)|\[\s*([\w-]+)\s*(.?=)?\s*("(?:\\.|[^"])+"|'(?:\\.|[^'])+'|(?:\\.|[^\]])*)\s*\]/g,function(t,_4b,_4c,_4d,_4e,_4f,_50){
if(_4d){
_4a=and(_4a,_1c[_4c||""](_4d.replace(/\\/g,"")));
}else{
if(_4b){
_4a=(_4b==" "?_3a:_3e)(_4a);
}else{
if(_4e){
_4a=and(_4a,_32(_4e,_50,_4f));
}
}
}
return "";
})){
throw new Error("Syntax error in query");
}
if(!_4a){
return true;
}
_42[_48]=_4a;
}
return _4a(_47,_49);
};
})();
}
if(!_1("dom-qsa")){
var _a=function(_51,_52){
var _53=_51.match(_6);
var _54=[];
for(var i=0;i<_53.length;i++){
_51=new String(_53[i].replace(/\s*$/,""));
_51.indexOf=escape;
var _55=_7(_51,_52);
for(var j=0,l=_55.length;j<l;j++){
var _56=_55[j];
_54[_56.sourceIndex]=_56;
}
}
var _57=[];
for(i in _54){
_57.push(_54[i]);
}
return _57;
};
}
_7.match=_4?function(_58,_59,_5a){
if(_5a&&_5a.nodeType!=9){
return _10(_5a,_59,function(_5b){
return _4.call(_58,_5b);
});
}
return _4.call(_58,_59);
}:_13;
return _7;
});
