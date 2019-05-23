/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Moveable",["../_base/array","../_base/declare","../_base/event","../_base/lang","../dom","../dom-class","../Evented","../on","../topic","../touch","./common","./Mover","../_base/window"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c){
var _d=_2("dojo.dnd.Moveable",[_7],{handle:"",delay:0,skip:false,constructor:function(_e,_f){
this.node=_5.byId(_e);
if(!_f){
_f={};
}
this.handle=_f.handle?_5.byId(_f.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.delay=_f.delay>0?_f.delay:0;
this.skip=_f.skip;
this.mover=_f.mover?_f.mover:_b;
this.events=[on(this.handle,_9.press,_4.hitch(this,"onMouseDown")),on(this.handle,"dragstart",_4.hitch(this,"onSelectStart")),on(this.handle,"selectstart",_4.hitch(this,"onSelectStart"))];
},markupFactory:function(_10,_11,_12){
return new _12(_11,_10);
},destroy:function(){
_1.forEach(this.events,function(_13){
_13.remove();
});
this.events=this.node=this.handle=null;
},onMouseDown:function(e){
if(this.skip&&_a.isFormElement(e)){
return;
}
if(this.delay){
this.events.push(on(this.handle,_9.move,_4.hitch(this,"onMouseMove")),on(this.handle,_9.release,_4.hitch(this,"onMouseUp")));
this._lastX=e.pageX;
this._lastY=e.pageY;
}else{
this.onDragDetected(e);
}
_3.stop(e);
},onMouseMove:function(e){
if(Math.abs(e.pageX-this._lastX)>this.delay||Math.abs(e.pageY-this._lastY)>this.delay){
this.onMouseUp(e);
this.onDragDetected(e);
}
_3.stop(e);
},onMouseUp:function(e){
for(var i=0;i<2;++i){
this.events.pop().remove();
}
_3.stop(e);
},onSelectStart:function(e){
if(!this.skip||!_a.isFormElement(e)){
_3.stop(e);
}
},onDragDetected:function(e){
new this.mover(this.node,e,this);
},onMoveStart:function(_14){
_8.publish("/dnd/move/start",_14);
_6.add(_c.body(),"dojoMove");
_6.add(this.node,"dojoMoveItem");
},onMoveStop:function(_15){
_8.publish("/dnd/move/stop",_15);
_6.remove(_c.body(),"dojoMove");
_6.remove(this.node,"dojoMoveItem");
},onFirstMove:function(){
},onMove:function(_16,_17){
this.onMoving(_16,_17);
var s=_16.node.style;
s.left=_17.l+"px";
s.top=_17.t+"px";
this.onMoved(_16,_17);
},onMoving:function(){
},onMoved:function(){
}});
return _d;
});
