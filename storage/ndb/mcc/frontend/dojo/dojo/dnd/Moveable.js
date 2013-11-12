/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Moveable",["../main","../Evented","../touch","./Mover"],function(_1,_2,_3){
_1.declare("dojo.dnd.Moveable",[_2],{handle:"",delay:0,skip:false,constructor:function(_4,_5){
this.node=_1.byId(_4);
if(!_5){
_5={};
}
this.handle=_5.handle?_1.byId(_5.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.delay=_5.delay>0?_5.delay:0;
this.skip=_5.skip;
this.mover=_5.mover?_5.mover:_1.dnd.Mover;
this.events=[_1.connect(this.handle,_3.press,this,"onMouseDown"),_1.connect(this.handle,"ondragstart",this,"onSelectStart"),_1.connect(this.handle,"onselectstart",this,"onSelectStart")];
},markupFactory:function(_6,_7,_8){
return new _8(_7,_6);
},destroy:function(){
_1.forEach(this.events,_1.disconnect);
this.events=this.node=this.handle=null;
},onMouseDown:function(e){
if(this.skip&&_1.dnd.isFormElement(e)){
return;
}
if(this.delay){
this.events.push(_1.connect(this.handle,_3.move,this,"onMouseMove"),_1.connect(this.handle,_3.release,this,"onMouseUp"));
this._lastX=e.pageX;
this._lastY=e.pageY;
}else{
this.onDragDetected(e);
}
_1.stopEvent(e);
},onMouseMove:function(e){
if(Math.abs(e.pageX-this._lastX)>this.delay||Math.abs(e.pageY-this._lastY)>this.delay){
this.onMouseUp(e);
this.onDragDetected(e);
}
_1.stopEvent(e);
},onMouseUp:function(e){
for(var i=0;i<2;++i){
_1.disconnect(this.events.pop());
}
_1.stopEvent(e);
},onSelectStart:function(e){
if(!this.skip||!_1.dnd.isFormElement(e)){
_1.stopEvent(e);
}
},onDragDetected:function(e){
new this.mover(this.node,e,this);
},onMoveStart:function(_9){
_1.publish("/dnd/move/start",[_9]);
_1.addClass(_1.body(),"dojoMove");
_1.addClass(this.node,"dojoMoveItem");
},onMoveStop:function(_a){
_1.publish("/dnd/move/stop",[_a]);
_1.removeClass(_1.body(),"dojoMove");
_1.removeClass(this.node,"dojoMoveItem");
},onFirstMove:function(_b,e){
},onMove:function(_c,_d,e){
this.onMoving(_c,_d);
var s=_c.node.style;
s.left=_d.l+"px";
s.top=_d.t+"px";
this.onMoved(_c,_d);
},onMoving:function(_e,_f){
},onMoved:function(_10,_11){
}});
return _1.dnd.Moveable;
});
