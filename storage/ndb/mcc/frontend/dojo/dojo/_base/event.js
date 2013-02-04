/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/event",["./kernel","../on","../has","../dom-geometry"],function(_1,on,_2,_3){
if(on._fixEvent){
var _4=on._fixEvent;
on._fixEvent=function(_5,se){
_5=_4(_5,se);
if(_5){
_3.normalizeEvent(_5);
}
return _5;
};
}
_1.fixEvent=function(_6,_7){
if(on._fixEvent){
return on._fixEvent(_6,_7);
}
return _6;
};
_1.stopEvent=function(_8){
if(_2("dom-addeventlistener")||(_8&&_8.preventDefault)){
_8.preventDefault();
_8.stopPropagation();
}else{
_8=_8||window.event;
_8.cancelBubble=true;
on._preventDefault.call(_8);
}
};
return {fix:_1.fixEvent,stop:_1.stopEvent};
});
