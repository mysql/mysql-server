/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/config",["../global","../has","require"],function(_1,_2,_3){
var _4={};
if(1){
var _5=_3.rawConfig,p;
for(p in _5){
_4[p]=_5[p];
}
}else{
var _6=function(_7,_8,_9){
for(p in _7){
p!="has"&&_2.add(_8+p,_7[p],0,_9);
}
};
_4=1?_3.rawConfig:_1.dojoConfig||_1.djConfig||{};
_6(_4,"config",1);
_6(_4.has,"",1);
}
if(!_4.locale&&typeof navigator!="undefined"){
var _a=(navigator.languages&&navigator.languages.length)?navigator.languages[0]:(navigator.language||navigator.userLanguage);
if(_a){
_4.locale=_a.toLowerCase();
}
}
return _4;
});
