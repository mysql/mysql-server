//>>built
define("dojox/gfx/Moveable",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/topic","dojo/touch","dojo/dom-class","dojo/_base/window","./Mover"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.gfx.Moveable",null,{constructor:function(_a,_b){
this.shape=_a;
this.delay=(_b&&_b.delay>0)?_b.delay:0;
this.mover=(_b&&_b.mover)?_b.mover:_9;
this.events=[this.shape.on(_6.press,_1.hitch(this,"onMouseDown"))];
},destroy:function(){
_3.forEach(this.events,function(_c){
_c.remove();
});
this.events=this.shape=null;
},onMouseDown:function(e){
if(this.delay){
this.events.push(this.shape.on(_6.move,_1.hitch(this,"onMouseMove")),this.shape.on(_6.release,_1.hitch(this,"onMouseUp")));
this._lastX=e.clientX;
this._lastY=e.clientY;
}else{
new this.mover(this.shape,e,this);
}
_4.stop(e);
},onMouseMove:function(e){
var _d=e.clientX,_e=e.clientY;
if(Math.abs(_d-this._lastX)>this.delay||Math.abs(_e-this._lastY)>this.delay){
this.onMouseUp(e);
new this.mover(this.shape,e,this);
}
_4.stop(e);
},onMouseUp:function(e){
this.events.pop().remove();
},onMoveStart:function(_f){
_5.publish("/gfx/move/start",_f);
_7.add(_8.body(),"dojoMove");
},onMoveStop:function(_10){
_5.publish("/gfx/move/stop",_10);
_7.remove(_8.body(),"dojoMove");
},onFirstMove:function(_11){
},onMove:function(_12,_13){
this.onMoving(_12,_13);
this.shape.applyLeftTransform(_13);
this.onMoved(_12,_13);
},onMoving:function(_14,_15){
},onMoved:function(_16,_17){
}});
});
