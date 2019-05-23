//>>built
define("dojox/gfx/Mover",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/on","dojo/has","dojo/_base/event"],function(_1,_2,_3,on,_4,_5){
return _3("dojox.gfx.Mover",null,{constructor:function(_6,e,_7){
this.shape=_6;
this.lastX=_4("touch")?(e.changedTouches?e.changedTouches[0]:e).clientX:e.clientX;
this.lastY=_4("touch")?(e.changedTouches?e.changedTouches[0]:e).clientY:e.clientY;
var h=this.host=_7,d=document,_8=on(d,_4("touch")?"touchmove":"mousemove",_1.hitch(this,"onFirstMove"));
this.events=[on(d,_4("touch")?"touchmove":"mousemove",_1.hitch(this,"onMouseMove")),on(d,_4("touch")?"touchend":"mouseup",_1.hitch(this,"destroy")),on(d,"dragstart",_1.hitch(_5,"stop")),on(d,"selectstart",_1.hitch(_5,"stop")),_8];
if(h&&h.onMoveStart){
h.onMoveStart(this);
}
},onMouseMove:function(e){
var x=_4("touch")?(e.changedTouches?e.changedTouches[0]:e).clientX:e.clientX;
var y=_4("touch")?(e.changedTouches?e.changedTouches[0]:e).clientY:e.clientY;
this.host.onMove(this,{dx:x-this.lastX,dy:y-this.lastY});
this.lastX=x;
this.lastY=y;
_5.stop(e);
},onFirstMove:function(){
this.host.onFirstMove(this);
this.events.pop().remove();
},destroy:function(){
_2.forEach(this.events,function(h){
h.remove();
});
var h=this.host;
if(h&&h.onMoveStop){
h.onMoveStop(this);
}
this.events=this.shape=null;
}});
});
