/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/move",["../main","./Mover","./Moveable"],function(_1){
_1.declare("dojo.dnd.move.constrainedMoveable",_1.dnd.Moveable,{constraints:function(){
},within:false,constructor:function(_2,_3){
if(!_3){
_3={};
}
this.constraints=_3.constraints;
this.within=_3.within;
},onFirstMove:function(_4){
var c=this.constraintBox=this.constraints.call(this,_4);
c.r=c.l+c.w;
c.b=c.t+c.h;
if(this.within){
var mb=_1._getMarginSize(_4.node);
c.r-=mb.w;
c.b-=mb.h;
}
},onMove:function(_5,_6){
var c=this.constraintBox,s=_5.node.style;
this.onMoving(_5,_6);
_6.l=_6.l<c.l?c.l:c.r<_6.l?c.r:_6.l;
_6.t=_6.t<c.t?c.t:c.b<_6.t?c.b:_6.t;
s.left=_6.l+"px";
s.top=_6.t+"px";
this.onMoved(_5,_6);
}});
_1.declare("dojo.dnd.move.boxConstrainedMoveable",_1.dnd.move.constrainedMoveable,{box:{},constructor:function(_7,_8){
var _9=_8&&_8.box;
this.constraints=function(){
return _9;
};
}});
_1.declare("dojo.dnd.move.parentConstrainedMoveable",_1.dnd.move.constrainedMoveable,{area:"content",constructor:function(_a,_b){
var _c=_b&&_b.area;
this.constraints=function(){
var n=this.node.parentNode,s=_1.getComputedStyle(n),mb=_1._getMarginBox(n,s);
if(_c=="margin"){
return mb;
}
var t=_1._getMarginExtents(n,s);
mb.l+=t.l,mb.t+=t.t,mb.w-=t.w,mb.h-=t.h;
if(_c=="border"){
return mb;
}
t=_1._getBorderExtents(n,s);
mb.l+=t.l,mb.t+=t.t,mb.w-=t.w,mb.h-=t.h;
if(_c=="padding"){
return mb;
}
t=_1._getPadExtents(n,s);
mb.l+=t.l,mb.t+=t.t,mb.w-=t.w,mb.h-=t.h;
return mb;
};
}});
_1.dnd.constrainedMover=_1.dnd.move.constrainedMover;
_1.dnd.boxConstrainedMover=_1.dnd.move.boxConstrainedMover;
_1.dnd.parentConstrainedMover=_1.dnd.move.parentConstrainedMover;
return _1.dnd.move;
});
