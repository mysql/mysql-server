/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/TimedMoveable",["../main","./Moveable"],function(_1){
var _2=_1.dnd.Moveable.prototype.onMove;
_1.declare("dojo.dnd.TimedMoveable",_1.dnd.Moveable,{timeout:40,constructor:function(_3,_4){
if(!_4){
_4={};
}
if(_4.timeout&&typeof _4.timeout=="number"&&_4.timeout>=0){
this.timeout=_4.timeout;
}
},onMoveStop:function(_5){
if(_5._timer){
clearTimeout(_5._timer);
_2.call(this,_5,_5._leftTop);
}
_1.dnd.Moveable.prototype.onMoveStop.apply(this,arguments);
},onMove:function(_6,_7){
_6._leftTop=_7;
if(!_6._timer){
var _8=this;
_6._timer=setTimeout(function(){
_6._timer=null;
_2.call(_8,_6,_6._leftTop);
},this.timeout);
}
}});
return _1.dnd.TimedMoveable;
});
