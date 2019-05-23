/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Manager",["../_base/array","../_base/declare","../_base/event","../_base/lang","../_base/window","../dom-class","../Evented","../has","../keys","../on","../topic","../touch","./common","./autoscroll","./Avatar"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c,_d,_e){
var _f=_2("dojo.dnd.Manager",[_7],{constructor:function(){
this.avatar=null;
this.source=null;
this.nodes=[];
this.copy=true;
this.target=null;
this.canDropFlag=false;
this.events=[];
},OFFSET_X:_8("touch")?4:16,OFFSET_Y:_8("touch")?4:16,overSource:function(_10){
if(this.avatar){
this.target=(_10&&_10.targetState!="Disabled")?_10:null;
this.canDropFlag=Boolean(this.target);
this.avatar.update();
}
_a.publish("/dnd/source/over",_10);
},outSource:function(_11){
if(this.avatar){
if(this.target==_11){
this.target=null;
this.canDropFlag=false;
this.avatar.update();
_a.publish("/dnd/source/over",null);
}
}else{
_a.publish("/dnd/source/over",null);
}
},startDrag:function(_12,_13,_14){
_d.autoScrollStart(_5.doc);
this.source=_12;
this.nodes=_13;
this.copy=Boolean(_14);
this.avatar=this.makeAvatar();
_5.body().appendChild(this.avatar.node);
_a.publish("/dnd/start",_12,_13,this.copy);
this.events=[on(_5.doc,_b.move,_4.hitch(this,"onMouseMove")),on(_5.doc,_b.release,_4.hitch(this,"onMouseUp")),on(_5.doc,"keydown",_4.hitch(this,"onKeyDown")),on(_5.doc,"keyup",_4.hitch(this,"onKeyUp")),on(_5.doc,"dragstart",_3.stop),on(_5.body(),"selectstart",_3.stop)];
var c="dojoDnd"+(_14?"Copy":"Move");
_6.add(_5.body(),c);
},canDrop:function(_15){
var _16=Boolean(this.target&&_15);
if(this.canDropFlag!=_16){
this.canDropFlag=_16;
this.avatar.update();
}
},stopDrag:function(){
_6.remove(_5.body(),["dojoDndCopy","dojoDndMove"]);
_1.forEach(this.events,function(_17){
_17.remove();
});
this.events=[];
this.avatar.destroy();
this.avatar=null;
this.source=this.target=null;
this.nodes=[];
},makeAvatar:function(){
return new _e(this);
},updateAvatar:function(){
this.avatar.update();
},onMouseMove:function(e){
var a=this.avatar;
if(a){
_d.autoScrollNodes(e);
var s=a.node.style;
s.left=(e.pageX+this.OFFSET_X)+"px";
s.top=(e.pageY+this.OFFSET_Y)+"px";
var _18=Boolean(this.source.copyState(_c.getCopyKeyState(e)));
if(this.copy!=_18){
this._setCopyStatus(_18);
}
}
if(_8("touch")){
e.preventDefault();
}
},onMouseUp:function(e){
if(this.avatar){
if(this.target&&this.canDropFlag){
var _19=Boolean(this.source.copyState(_c.getCopyKeyState(e)));
_a.publish("/dnd/drop/before",this.source,this.nodes,_19,this.target,e);
_a.publish("/dnd/drop",this.source,this.nodes,_19,this.target,e);
}else{
_a.publish("/dnd/cancel");
}
this.stopDrag();
}
},onKeyDown:function(e){
if(this.avatar){
switch(e.keyCode){
case _9.CTRL:
var _1a=Boolean(this.source.copyState(true));
if(this.copy!=_1a){
this._setCopyStatus(_1a);
}
break;
case _9.ESCAPE:
_a.publish("/dnd/cancel");
this.stopDrag();
break;
}
}
},onKeyUp:function(e){
if(this.avatar&&e.keyCode==_9.CTRL){
var _1b=Boolean(this.source.copyState(false));
if(this.copy!=_1b){
this._setCopyStatus(_1b);
}
}
},_setCopyStatus:function(_1c){
this.copy=_1c;
this.source._markDndStatus(this.copy);
this.updateAvatar();
_6.replace(_5.body(),"dojoDnd"+(this.copy?"Copy":"Move"),"dojoDnd"+(this.copy?"Move":"Copy"));
}});
_c._manager=null;
_f.manager=_c.manager=function(){
if(!_c._manager){
_c._manager=new _f();
}
return _c._manager;
};
return _f;
});
