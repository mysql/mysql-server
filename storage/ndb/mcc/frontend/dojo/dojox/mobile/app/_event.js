//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.mobile.app._event");
_2.experimental("dojox.mobile.app._event.js");
_2.mixin(_3.mobile.app,{eventMap:{},connectFlick:function(_4,_5,_6){
var _7;
var _8;
var _9=false;
var _a;
var _b;
var _c;
var _d;
var _e;
var _f;
var _10=_2.connect("onmousedown",_4,function(_11){
_9=false;
_7=_11.targetTouches?_11.targetTouches[0].clientX:_11.clientX;
_8=_11.targetTouches?_11.targetTouches[0].clientY:_11.clientY;
_f=(new Date()).getTime();
_c=_2.connect(_4,"onmousemove",_12);
_d=_2.connect(_4,"onmouseup",_13);
});
var _12=function(_14){
_2.stopEvent(_14);
_a=_14.targetTouches?_14.targetTouches[0].clientX:_14.clientX;
_b=_14.targetTouches?_14.targetTouches[0].clientY:_14.clientY;
if(Math.abs(Math.abs(_a)-Math.abs(_7))>15){
_9=true;
_e=(_a>_7)?"ltr":"rtl";
}else{
if(Math.abs(Math.abs(_b)-Math.abs(_8))>15){
_9=true;
_e=(_b>_8)?"ttb":"btt";
}
}
};
var _13=function(_15){
_2.stopEvent(_15);
_c&&_2.disconnect(_c);
_d&&_2.disconnect(_d);
if(_9){
var _16={target:_4,direction:_e,duration:(new Date()).getTime()-_f};
if(_5&&_6){
_5[_6](_16);
}else{
_6(_16);
}
}
};
}});
_3.mobile.app.isIPhone=(_2.isSafari&&(navigator.userAgent.indexOf("iPhone")>-1||navigator.userAgent.indexOf("iPod")>-1));
_3.mobile.app.isWebOS=(navigator.userAgent.indexOf("webOS")>-1);
_3.mobile.app.isAndroid=(navigator.userAgent.toLowerCase().indexOf("android")>-1);
if(_3.mobile.app.isIPhone||_3.mobile.app.isAndroid){
_3.mobile.app.eventMap={onmousedown:"ontouchstart",mousedown:"ontouchstart",onmouseup:"ontouchend",mouseup:"ontouchend",onmousemove:"ontouchmove",mousemove:"ontouchmove"};
}
_2._oldConnect=_2._connect;
_2._connect=function(obj,_17,_18,_19,_1a){
_17=_3.mobile.app.eventMap[_17]||_17;
if(_17=="flick"||_17=="onflick"){
if(_2.global["Mojo"]){
_17=Mojo.Event.flick;
}else{
return _3.mobile.app.connectFlick(obj,_18,_19);
}
}
return _2._oldConnect(obj,_17,_18,_19,_1a);
};
});
