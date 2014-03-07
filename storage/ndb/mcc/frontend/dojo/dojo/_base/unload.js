/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/unload",["./kernel","./connect"],function(_1,_2){
var _3=window;
_1.addOnWindowUnload=function(_4,_5){
if(!_1.windowUnloaded){
_2.connect(_3,"unload",(_1.windowUnloaded=function(){
}));
}
_2.connect(_3,"unload",_4,_5);
};
_1.addOnUnload=function(_6,_7){
_2.connect(_3,"beforeunload",_6,_7);
};
return {addOnWindowUnload:_1.addOnWindowUnload,addOnUnload:_1.addOnUnload};
});
