//>>built
define("dijit/Viewport",["dojo/Evented","dojo/on","dojo/ready","dojo/sniff","dojo/_base/window","dojo/window"],function(_1,on,_2,_3,_4,_5){
var _6=new _1();
var _7;
_2(200,function(){
var _8=_5.getBox();
_6._rlh=on(_4.global,"resize",function(){
var _9=_5.getBox();
if(_8.h==_9.h&&_8.w==_9.w){
return;
}
_8=_9;
_6.emit("resize");
});
if(_3("ie")==8){
var _a=screen.deviceXDPI;
setInterval(function(){
if(screen.deviceXDPI!=_a){
_a=screen.deviceXDPI;
_6.emit("resize");
}
},500);
}
if(_3("ios")){
on(document,"focusin",function(_b){
_7=_b.target;
});
on(document,"focusout",function(_c){
_7=null;
});
}
});
_6.getEffectiveBox=function(_d){
var _e=_5.getBox(_d);
var _f=_7&&_7.tagName&&_7.tagName.toLowerCase();
if(_3("ios")&&_7&&!_7.readOnly&&(_f=="textarea"||(_f=="input"&&/^(color|email|number|password|search|tel|text|url)$/.test(_7.type)))){
_e.h*=(orientation==0||orientation==180?0.66:0.4);
var _10=_7.getBoundingClientRect();
_e.h=Math.max(_e.h,_10.top+_10.height);
}
return _e;
};
return _6;
});
