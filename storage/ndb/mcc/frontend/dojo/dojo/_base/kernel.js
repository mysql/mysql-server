/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/kernel",["../has","./config","require","module"],function(_1,_2,_3,_4){
var i,p,_5=(function(){
return this;
})(),_6={},_7={},_8={config:_2,global:_5,dijit:_6,dojox:_7};
var _9={dojo:["dojo",_8],dijit:["dijit",_6],dojox:["dojox",_7]},_a=(_3.map&&_3.map[_4.id.match(/[^\/]+/)[0]]),_b;
for(p in _a){
if(_9[p]){
_9[p][0]=_a[p];
}else{
_9[p]=[_a[p],{}];
}
}
for(p in _9){
_b=_9[p];
_b[1]._scopeName=_b[0];
if(!_2.noGlobals){
_5[_b[0]]=_b[1];
}
}
_8.scopeMap=_9;
_8.baseUrl=_8.config.baseUrl=_3.baseUrl;
_8.isAsync=!1||_3.async;
_8.locale=_2.locale;
var _c="$Rev: 18f4d48 $".match(/[0-9a-f]{7,}/);
_8.version={major:1,minor:8,patch:14,flag:"",revision:_c?_c[0]:NaN,toString:function(){
var v=_8.version;
return v.major+"."+v.minor+"."+v.patch+v.flag+" ("+v.revision+")";
}};
1||_1.add("extend-dojo",1);
(Function("d","d.eval = function(){return d.global.eval ? d.global.eval(arguments[0]) : eval(arguments[0]);}"))(_8);
if(0){
_8.exit=function(_d){
quit(_d);
};
}else{
_8.exit=function(){
};
}
1||_1.add("dojo-guarantee-console",1);
if(1){
_1.add("console-as-object",function(){
return Function.prototype.bind&&console&&typeof console.log==="object";
});
typeof console!="undefined"||(console={});
var cn=["assert","count","debug","dir","dirxml","error","group","groupEnd","info","profile","profileEnd","time","timeEnd","trace","warn","log"];
var tn;
i=0;
while((tn=cn[i++])){
if(!console[tn]){
(function(){
var _e=tn+"";
console[_e]=("log" in console)?function(){
var a=Array.prototype.slice.call(arguments);
a.unshift(_e+":");
console["log"](a.join(" "));
}:function(){
};
console[_e]._fake=true;
})();
}else{
if(_1("console-as-object")){
console[tn]=Function.prototype.bind.call(console[tn],console);
}
}
}
}
_1.add("dojo-debug-messages",!!_2.isDebug);
_8.deprecated=_8.experimental=function(){
};
if(_1("dojo-debug-messages")){
_8.deprecated=function(_f,_10,_11){
var _12="DEPRECATED: "+_f;
if(_10){
_12+=" "+_10;
}
if(_11){
_12+=" -- will be removed in version: "+_11;
}
console.warn(_12);
};
_8.experimental=function(_13,_14){
var _15="EXPERIMENTAL: "+_13+" -- APIs subject to change without notice.";
if(_14){
_15+=" "+_14;
}
console.warn(_15);
};
}
1||_1.add("dojo-modulePaths",1);
if(1){
if(_2.modulePaths){
_8.deprecated("dojo.modulePaths","use paths configuration");
var _16={};
for(p in _2.modulePaths){
_16[p.replace(/\./g,"/")]=_2.modulePaths[p];
}
_3({paths:_16});
}
}
1||_1.add("dojo-moduleUrl",1);
if(1){
_8.moduleUrl=function(_17,url){
_8.deprecated("dojo.moduleUrl()","use require.toUrl","2.0");
var _18=null;
if(_17){
_18=_3.toUrl(_17.replace(/\./g,"/")+(url?("/"+url):"")+"/*.*").replace(/\/\*\.\*/,"")+(url?"":"/");
}
return _18;
};
}
_8._hasResource={};
return _8;
});
