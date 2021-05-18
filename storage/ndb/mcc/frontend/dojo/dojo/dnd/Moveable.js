/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Moveable",["../_base/array","../_base/declare","../_base/lang","../dom","../dom-class","../Evented","../has","../on","../topic","../touch","./common","./Mover","../_base/window"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c){
var _d;
var _e=function(){
};
function _f(){
if("touchAction" in document.body.style){
_d="touchAction";
}else{
if("msTouchAction" in document.body.style){
_d="msTouchAction";
}
}
_e=function _e(_10,_11){
_10.style[_d]=_11;
};
_e(arguments[0],arguments[1]);
};
if(_7("touch-action")){
_e=_f;
}
var _12=_2("dojo.dnd.Moveable",[_6],{handle:"",delay:0,skip:false,constructor:function(_13,_14){
this.node=_4.byId(_13);
_e(this.node,"none");
if(!_14){
_14={};
}
this.handle=_14.handle?_4.byId(_14.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.delay=_14.delay>0?_14.delay:0;
this.skip=_14.skip;
this.mover=_14.mover?_14.mover:_b;
this.events=[on(this.handle,_9.press,_3.hitch(this,"onMouseDown")),on(this.handle,"dragstart",_3.hitch(this,"onSelectStart")),on(this.handle,"selectstart",_3.hitch(this,"onSelectStart"))];
},markupFactory:function(_15,_16,_17){
return new _17(_16,_15);
},destroy:function(){
_1.forEach(this.events,function(_18){
_18.remove();
});
_e(this.node,"");
this.events=this.node=this.handle=null;
},onMouseDown:function(e){
if(this.skip&&_a.isFormElement(e)){
return;
}
if(this.delay){
this.events.push(on(this.handle,_9.move,_3.hitch(this,"onMouseMove")),on(this.handle.ownerDocument,_9.release,_3.hitch(this,"onMouseUp")));
this._lastX=e.pageX;
this._lastY=e.pageY;
}else{
this.onDragDetected(e);
}
e.stopPropagation();
e.preventDefault();
},onMouseMove:function(e){
if(Math.abs(e.pageX-this._lastX)>this.delay||Math.abs(e.pageY-this._lastY)>this.delay){
this.onMouseUp(e);
this.onDragDetected(e);
}
e.stopPropagation();
e.preventDefault();
},onMouseUp:function(e){
for(var i=0;i<2;++i){
this.events.pop().remove();
}
e.stopPropagation();
e.preventDefault();
},onSelectStart:function(e){
if(!this.skip||!_a.isFormElement(e)){
e.stopPropagation();
e.preventDefault();
}
},onDragDetected:function(e){
new this.mover(this.node,e,this);
},onMoveStart:function(_19){
_8.publish("/dnd/move/start",_19);
_5.add(_c.body(),"dojoMove");
_5.add(this.node,"dojoMoveItem");
},onMoveStop:function(_1a){
_8.publish("/dnd/move/stop",_1a);
_5.remove(_c.body(),"dojoMove");
_5.remove(this.node,"dojoMoveItem");
},onFirstMove:function(){
},onMove:function(_1b,_1c){
this.onMoving(_1b,_1c);
var s=_1b.node.style;
s.left=_1c.l+"px";
s.top=_1c.t+"px";
this.onMoved(_1b,_1c);
},onMoving:function(){
},onMoved:function(){
}});
return _12;
});
