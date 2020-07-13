//>>built
define("dojox/gfx/Mover",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/on","dojo/touch","dojo/_base/event"],function(_1,_2,_3,on,_4,_5){
return _3("dojox.gfx.Mover",null,{constructor:function(_6,e,_7){
this.shape=_6;
this.lastX=e.clientX;
this.lastY=e.clientY;
var h=this.host=_7,d=document,_8=on(d,_4.move,_1.hitch(this,"onFirstMove"));
this.events=[on(d,_4.move,_1.hitch(this,"onMouseMove")),on(d,_4.release,_1.hitch(this,"destroy")),on(d,"dragstart",_1.hitch(_5,"stop")),on(d,"selectstart",_1.hitch(_5,"stop")),_8];
if(h&&h.onMoveStart){
h.onMoveStart(this);
}
},onMouseMove:function(e){
var x=e.clientX;
var y=e.clientY;
this.host.onMove(this,{dx:x-this.lastX,dy:y-this.lastY});
this.lastX=x;
this.lastY=y;
_5.stop(e);
},onFirstMove:function(){
this.host.onFirstMove(this);
this.events.pop().remove();
},destroy:function(){
_2.forEach(this.events,function(_9){
_9.remove();
});
var h=this.host;
if(h&&h.onMoveStop){
h.onMoveStop(this);
}
this.events=this.shape=null;
}});
});
