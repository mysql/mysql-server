//>>built
define("dojox/gesture/swipe",["dojo/_base/kernel","dojo/_base/declare","./Base","../main"],function(_1,_2,_3,_4){
_1.experimental("dojox.gesture.swipe");
var _5=_2(_3,{defaultEvent:"swipe",subEvents:["end"],press:function(_6,e){
if(e.touches&&e.touches.length>=2){
delete _6.context;
return;
}
if(!_6.context){
_6.context={x:0,y:0,t:0};
}
_6.context.x=e.screenX;
_6.context.y=e.screenY;
_6.context.t=new Date().getTime();
this.lock(e.currentTarget);
},move:function(_7,e){
this._recognize(_7,e,"swipe");
},release:function(_8,e){
this._recognize(_8,e,"swipe.end");
delete _8.context;
this.unLock();
},cancel:function(_9,e){
delete _9.context;
this.unLock();
},_recognize:function(_a,e,_b){
if(!_a.context){
return;
}
var _c=this._getSwipeInfo(_a,e);
if(!_c){
return;
}
_c.type=_b;
this.fire(e.target,_c);
},_getSwipeInfo:function(_d,e){
var dx,dy,_e={},_f=_d.context;
_e.time=new Date().getTime()-_f.t;
dx=e.screenX-_f.x;
dy=e.screenY-_f.y;
if(dx===0&&dy===0){
return null;
}
_e.dx=dx;
_e.dy=dy;
return _e;
}});
_4.gesture.swipe=new _5();
_4.gesture.swipe.Swipe=_5;
return _4.gesture.swipe;
});
