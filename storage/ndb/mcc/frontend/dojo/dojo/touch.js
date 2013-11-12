/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/touch",["./_base/kernel","./on","./has","./mouse"],function(_1,on,_2,_3){
function _4(_5){
return function(_6,_7){
return on(_6,_5,_7);
};
};
var _8=_2("touch");
_1.touch={press:_4(_8?"touchstart":"mousedown"),move:_4(_8?"touchmove":"mousemove"),release:_4(_8?"touchend":"mouseup"),cancel:_8?_4("touchcancel"):_3.leave};
return _1.touch;
});
