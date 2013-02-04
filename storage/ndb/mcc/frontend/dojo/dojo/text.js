/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/text",["./_base/kernel","require","./has","./_base/xhr"],function(_1,_2,_3,_4){
var _5;
if(1){
_5=function(_6,_7,_8){
_4("GET",{url:_6,sync:!!_7,load:_8});
};
}else{
if(_2.getText){
_5=_2.getText;
}else{
console.error("dojo/text plugin failed to load because loader does not support getText");
}
}
var _9={},_a=function(_b){
if(_b){
_b=_b.replace(/^\s*<\?xml(\s)+version=[\'\"](\d)*.(\d)*[\'\"](\s)*\?>/im,"");
var _c=_b.match(/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im);
if(_c){
_b=_c[1];
}
}else{
_b="";
}
return _b;
},_d={},_e={},_f={dynamic:true,normalize:function(id,_10){
var _11=id.split("!"),url=_11[0];
return (/^\./.test(url)?_10(url):url)+(_11[1]?"!"+_11[1]:"");
},load:function(id,_12,_13){
var _14=id.split("!"),_15=_14.length>1,_16=_14[0],url=_12.toUrl(_14[0]),_17=_d,_18=function(_19){
_13(_15?_a(_19):_19);
};
if(_16 in _9){
_17=_9[_16];
}else{
if(url in _12.cache){
_17=_12.cache[url];
}else{
if(url in _9){
_17=_9[url];
}
}
}
if(_17===_d){
if(_e[url]){
_e[url].push(_18);
}else{
var _1a=_e[url]=[_18];
_5(url,!_12.async,function(_1b){
_9[_16]=_9[url]=_1b;
for(var i=0;i<_1a.length;){
_1a[i++](_1b);
}
delete _e[url];
});
}
}else{
_18(_17);
}
}};
_1.cache=function(_1c,url,_1d){
var key;
if(typeof _1c=="string"){
if(/\//.test(_1c)){
key=_1c;
_1d=url;
}else{
key=_2.toUrl(_1c.replace(/\./g,"/")+(url?("/"+url):""));
}
}else{
key=_1c+"";
_1d=url;
}
var val=(_1d!=undefined&&typeof _1d!="string")?_1d.value:_1d,_1e=_1d&&_1d.sanitize;
if(typeof val=="string"){
_9[key]=val;
return _1e?_a(val):val;
}else{
if(val===null){
delete _9[key];
return null;
}else{
if(!(key in _9)){
_5(key,true,function(_1f){
_9[key]=_1f;
});
}
return _1e?_a(_9[key]):_9[key];
}
}
};
return _f;
});
