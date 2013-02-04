/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/kernel",["../has","./config","require","module"],function(_1,_2,_3,_4){
var i,p,_5={},_6={},_7={config:_2,global:this,dijit:_5,dojox:_6};
var _8={dojo:["dojo",_7],dijit:["dijit",_5],dojox:["dojox",_6]},_9=(_3.packs&&_3.packs[_4.id.match(/[^\/]+/)[0]].packageMap)||{},_a;
for(p in _9){
if(_8[p]){
_8[p][0]=_9[p];
}else{
_8[p]=[_9[p],{}];
}
}
for(p in _8){
_a=_8[p];
_a[1]._scopeName=_a[0];
if(!_2.noGlobals){
this[_a[0]]=_a[1];
}
}
_7.scopeMap=_8;
_7.baseUrl=_7.config.baseUrl=_3.baseUrl;
_7.isAsync=!1||_3.async;
_7.locale=_2.locale;
var _b="$Rev: 27407 $".match(/\d+/);
_7.version={major:1,minor:7,patch:1,flag:"",revision:_b?+_b[0]:NaN,toString:function(){
var v=_7.version;
return v.major+"."+v.minor+"."+v.patch+v.flag+" ("+v.revision+")";
}};
true||_1.add("extend-dojo",1);
if(1){
_7.eval=_3.eval;
}else{
var _c=new Function("__text","return eval(__text);");
_7.eval=function(_d,_e){
return _c(_d+"\r\n////@ sourceURL="+_e);
};
}
if(0){
_7.exit=function(_f){
quit(_f);
};
}else{
_7.exit=function(){
};
}
true||_1.add("dojo-guarantee-console",1);
if(1){
typeof console!="undefined"||(console={});
var cn=["assert","count","debug","dir","dirxml","error","group","groupEnd","info","profile","profileEnd","time","timeEnd","trace","warn","log"];
var tn;
i=0;
while((tn=cn[i++])){
if(!console[tn]){
(function(){
var tcn=tn+"";
console[tcn]=("log" in console)?function(){
var a=Array.apply({},arguments);
a.unshift(tcn+":");
console["log"](a.join(" "));
}:function(){
};
console[tcn]._fake=true;
})();
}
}
}
_1.add("dojo-debug-messages",!!_2.isDebug);
if(_1("dojo-debug-messages")){
_7.deprecated=function(_10,_11,_12){
var _13="DEPRECATED: "+_10;
if(_11){
_13+=" "+_11;
}
if(_12){
_13+=" -- will be removed in version: "+_12;
}
console.warn(_13);
};
_7.experimental=function(_14,_15){
var _16="EXPERIMENTAL: "+_14+" -- APIs subject to change without notice.";
if(_15){
_16+=" "+_15;
}
console.warn(_16);
};
}else{
_7.deprecated=_7.experimental=function(){
};
}
true||_1.add("dojo-modulePaths",1);
if(1){
if(_2.modulePaths){
_7.deprecated("dojo.modulePaths","use paths configuration");
var _17={};
for(p in _2.modulePaths){
_17[p.replace(/\./g,"/")]=_2.modulePaths[p];
}
_3({paths:_17});
}
}
true||_1.add("dojo-moduleUrl",1);
if(1){
_7.moduleUrl=function(_18,url){
_7.deprecated("dojo.moduleUrl()","use require.toUrl","2.0");
var _19=null;
if(_18){
_19=_3.toUrl(_18.replace(/\./g,"/")+(url?("/"+url):"")+"/*.*").replace(/\/\*\.\*/,"")+(url?"":"/");
}
return _19;
};
}
_7._hasResource={};
return _7;
});
