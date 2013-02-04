//>>built
define("dojox/gfx/Mover",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/connect","dojo/_base/event"],function(_1,_2,_3,_4,_5){
return _3("dojox.gfx.Mover",null,{constructor:function(_6,e,_7){
this.shape=_6;
this.lastX=e.clientX;
this.lastY=e.clientY;
var h=this.host=_7,d=document,_8=_4.connect(d,"onmousemove",this,"onFirstMove");
this.events=[_4.connect(d,"onmousemove",this,"onMouseMove"),_4.connect(d,"onmouseup",this,"destroy"),_4.connect(d,"ondragstart",_5,"stop"),_4.connect(d,"onselectstart",_5,"stop"),_8];
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
_4.disconnect(this.events.pop());
},destroy:function(){
_2.forEach(this.events,_4.disconnect);
var h=this.host;
if(h&&h.onMoveStop){
h.onMoveStop(this);
}
this.events=this.shape=null;
}});
});
