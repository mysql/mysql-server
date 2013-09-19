//>>built
define("dojox/gesture/tap",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","./Base","../main"],function(_1,_2,_3,_4,_5){
_1.experimental("dojox.gesture.tap");
var _6=_2(_4,{defaultEvent:"tap",subEvents:["hold","doubletap"],holdThreshold:500,doubleTapTimeout:250,tapRadius:10,press:function(_7,e){
if(e.touches&&e.touches.length>=2){
delete _7.context;
return;
}
var _8=e.target;
this._initTap(_7,e);
_7.tapTimeOut=setTimeout(_3.hitch(this,function(){
if(this._isTap(_7,e)){
this.fire(_8,{type:"tap.hold"});
}
delete _7.context;
}),this.holdThreshold);
},release:function(_9,e){
if(!_9.context){
clearTimeout(_9.tapTimeOut);
return;
}
if(this._isTap(_9,e)){
switch(_9.context.c){
case 1:
this.fire(e.target,{type:"tap"});
break;
case 2:
this.fire(e.target,{type:"tap.doubletap"});
break;
}
}
clearTimeout(_9.tapTimeOut);
},_initTap:function(_a,e){
if(!_a.context){
_a.context={x:0,y:0,t:0,c:0};
}
var ct=new Date().getTime();
if(ct-_a.context.t<=this.doubleTapTimeout){
_a.context.c++;
}else{
_a.context.c=1;
_a.context.x=e.screenX;
_a.context.y=e.screenY;
}
_a.context.t=ct;
},_isTap:function(_b,e){
var dx=Math.abs(_b.context.x-e.screenX);
var dy=Math.abs(_b.context.y-e.screenY);
return dx<=this.tapRadius&&dy<=this.tapRadius;
}});
_5.gesture.tap=new _6();
_5.gesture.tap.Tap=_6;
return _5.gesture.tap;
});
