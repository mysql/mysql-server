/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/main",["./_base/kernel","./has","require","./_base/sniff","./_base/lang","./_base/array","./ready","./_base/declare","./_base/connect","./_base/Deferred","./_base/json","./_base/Color","./has!dojo-firebug?./_firebug/firebug","./_base/browser","./_base/loader"],function(_1,_2,_3,_4,_5,_6,_7){
if(_1.config.isDebug){
_3(["./_firebug/firebug"]);
}
true||_2.add("dojo-config-require",1);
if(1){
var _8=_1.config.require;
if(_8){
_8=_6.map(_5.isArray(_8)?_8:[_8],function(_9){
return _9.replace(/\./g,"/");
});
if(_1.isAsync){
_3(_8);
}else{
_7(1,function(){
_3(_8);
});
}
}
}
return _1;
});
