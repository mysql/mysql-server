//>>built
define("dojox/gfx/Moveable",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/topic","dojo/has","dojo/dom-class","dojo/_base/window","./Mover"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.gfx.Moveable",null,{constructor:function(_a,_b){
this.shape=_a;
this.delay=(_b&&_b.delay>0)?_b.delay:0;
this.mover=(_b&&_b.mover)?_b.mover:_9;
this.events=[this.shape.connect(_6("touch")?"touchstart":"mousedown",this,"onMouseDown")];
},destroy:function(){
_3.forEach(this.events,this.shape.disconnect,this.shape);
this.events=this.shape=null;
},onMouseDown:function(e){
if(this.delay){
this.events.push(this.shape.connect(_6("touch")?"touchmove":"mousemove",this,"onMouseMove"),this.shape.connect(_6("touch")?"touchend":"mouseup",this,"onMouseUp"));
this._lastX=_6("touch")?(e.changedTouches?e.changedTouches[0]:e).clientX:e.clientX;
this._lastY=_6("touch")?(e.changedTouches?e.changedTouches[0]:e).clientY:e.clientY;
}else{
new this.mover(this.shape,e,this);
}
_4.stop(e);
},onMouseMove:function(e){
var _c=_6("touch")?(e.changedTouches?e.changedTouches[0]:e).clientX:e.clientX,_d=_6("touch")?(e.changedTouches?e.changedTouches[0]:e).clientY:e.clientY;
if(Math.abs(_c-this._lastX)>this.delay||Math.abs(_d-this._lastY)>this.delay){
this.onMouseUp(e);
new this.mover(this.shape,e,this);
}
_4.stop(e);
},onMouseUp:function(e){
this.shape.disconnect(this.events.pop());
},onMoveStart:function(_e){
_5.publish("/gfx/move/start",_e);
_7.add(_8.body(),"dojoMove");
},onMoveStop:function(_f){
_5.publish("/gfx/move/stop",_f);
_7.remove(_8.body(),"dojoMove");
},onFirstMove:function(_10){
},onMove:function(_11,_12){
this.onMoving(_11,_12);
this.shape.applyLeftTransform(_12);
this.onMoved(_11,_12);
},onMoving:function(_13,_14){
},onMoved:function(_15,_16){
}});
});
