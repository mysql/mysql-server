/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/request/handlers",["../json","../_base/kernel","../_base/array","../has","../selector/_loader"],function(_1,_2,_3,_4){
_4.add("activex",typeof ActiveXObject!=="undefined");
_4.add("dom-parser",function(_5){
return "DOMParser" in _5;
});
var _6;
if(_4("activex")){
var dp=["Msxml2.DOMDocument.6.0","Msxml2.DOMDocument.4.0","MSXML2.DOMDocument.3.0","MSXML.DOMDocument"];
var _7;
_6=function(_8){
var _9=_8.data;
var _a=_8.text;
if(_9&&_4("dom-qsa2.1")&&!_9.querySelectorAll&&_4("dom-parser")){
_9=new DOMParser().parseFromString(_a,"application/xml");
}
function _b(p){
try{
var _c=new ActiveXObject(p);
_c.async=false;
_c.loadXML(_a);
_9=_c;
_7=p;
}
catch(e){
return false;
}
return true;
};
if(!_9||!_9.documentElement){
if(!_7||!_b(_7)){
_3.some(dp,_b);
}
}
return _9;
};
}
var _d=function(_e){
if(!_4("native-xhr2-blob")&&_e.options.handleAs==="blob"&&typeof Blob!=="undefined"){
return new Blob([_e.xhr.response],{type:_e.xhr.getResponseHeader("Content-Type")});
}
return _e.xhr.response;
};
var _f={"javascript":function(_10){
return _2.eval(_10.text||"");
},"json":function(_11){
return _1.parse(_11.text||null);
},"xml":_6,"blob":_d,"arraybuffer":_d,"document":_d};
function _12(_13){
var _14=_f[_13.options.handleAs];
_13.data=_14?_14(_13):(_13.data||_13.text);
return _13;
};
_12.register=function(_15,_16){
_f[_15]=_16;
};
return _12;
});
