/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/touch",["./_base/kernel","./aspect","./dom","./on","./has","./mouse","./domReady","./_base/window"],function(_1,_2,_3,on,_4,_5,_6,_7){
var _8=_4("touch");
var _9=false;
if(_4("ios")){
var ua=navigator.userAgent;
var v=ua.match(/OS ([\d_]+)/)?RegExp.$1:"1";
var os=parseFloat(v.replace(/_/,".").replace(/_/g,""));
_9=os<5;
}
var _a;
function _b(_c,_d){
if(_8){
return function(_e,_f){
var _10=on(_e,_d,_f),_11=on(_e,_c,function(evt){
if(!_a||(new Date()).getTime()>_a+1000){
_f.call(this,evt);
}
});
return {remove:function(){
_10.remove();
_11.remove();
}};
};
}else{
return function(_12,_13){
return on(_12,_c,_13);
};
}
};
var _14,_15;
if(_8){
_6(function(){
_15=_7.body();
_7.doc.addEventListener("touchstart",function(evt){
_a=(new Date()).getTime();
var _16=_15;
_15=evt.target;
on.emit(_16,"dojotouchout",{target:_16,relatedTarget:_15,bubbles:true});
on.emit(_15,"dojotouchover",{target:_15,relatedTarget:_16,bubbles:true});
},true);
on(_7.doc,"touchmove",function(evt){
_a=(new Date()).getTime();
var _17=_7.doc.elementFromPoint(evt.pageX-(_9?0:_7.global.pageXOffset),evt.pageY-(_9?0:_7.global.pageYOffset));
if(_17&&_15!==_17){
on.emit(_15,"dojotouchout",{target:_15,relatedTarget:_17,bubbles:true});
on.emit(_17,"dojotouchover",{target:_17,relatedTarget:_15,bubbles:true});
_15=_17;
}
});
});
_14=function(_18,_19){
return on(_7.doc,"touchmove",function(evt){
if(_18===_7.doc||_3.isDescendant(_15,_18)){
evt.target=_15;
_19.call(this,evt);
}
});
};
}
var _1a={press:_b("mousedown","touchstart"),move:_b("mousemove",_14),release:_b("mouseup","touchend"),cancel:_b(_5.leave,"touchcancel"),over:_b("mouseover","dojotouchover"),out:_b("mouseout","dojotouchout"),enter:_5._eventHandler(_b("mouseover","dojotouchover")),leave:_5._eventHandler(_b("mouseout","dojotouchout"))};
1&&(_1.touch=_1a);
return _1a;
});
