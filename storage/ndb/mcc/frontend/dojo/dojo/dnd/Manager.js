/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Manager",["../main","../Evented","./common","./autoscroll","./Avatar"],function(_1,_2){
var _3=_1.declare("dojo.dnd.Manager",[_2],{constructor:function(){
this.avatar=null;
this.source=null;
this.nodes=[];
this.copy=true;
this.target=null;
this.canDropFlag=false;
this.events=[];
},OFFSET_X:16,OFFSET_Y:16,overSource:function(_4){
if(this.avatar){
this.target=(_4&&_4.targetState!="Disabled")?_4:null;
this.canDropFlag=Boolean(this.target);
this.avatar.update();
}
_1.publish("/dnd/source/over",[_4]);
},outSource:function(_5){
if(this.avatar){
if(this.target==_5){
this.target=null;
this.canDropFlag=false;
this.avatar.update();
_1.publish("/dnd/source/over",[null]);
}
}else{
_1.publish("/dnd/source/over",[null]);
}
},startDrag:function(_6,_7,_8){
this.source=_6;
this.nodes=_7;
this.copy=Boolean(_8);
this.avatar=this.makeAvatar();
_1.body().appendChild(this.avatar.node);
_1.publish("/dnd/start",[_6,_7,this.copy]);
this.events=[_1.connect(_1.doc,"onmousemove",this,"onMouseMove"),_1.connect(_1.doc,"onmouseup",this,"onMouseUp"),_1.connect(_1.doc,"onkeydown",this,"onKeyDown"),_1.connect(_1.doc,"onkeyup",this,"onKeyUp"),_1.connect(_1.doc,"ondragstart",_1.stopEvent),_1.connect(_1.body(),"onselectstart",_1.stopEvent)];
var c="dojoDnd"+(_8?"Copy":"Move");
_1.addClass(_1.body(),c);
},canDrop:function(_9){
var _a=Boolean(this.target&&_9);
if(this.canDropFlag!=_a){
this.canDropFlag=_a;
this.avatar.update();
}
},stopDrag:function(){
_1.removeClass(_1.body(),["dojoDndCopy","dojoDndMove"]);
_1.forEach(this.events,_1.disconnect);
this.events=[];
this.avatar.destroy();
this.avatar=null;
this.source=this.target=null;
this.nodes=[];
},makeAvatar:function(){
return new _1.dnd.Avatar(this);
},updateAvatar:function(){
this.avatar.update();
},onMouseMove:function(e){
var a=this.avatar;
if(a){
_1.dnd.autoScrollNodes(e);
var s=a.node.style;
s.left=(e.pageX+this.OFFSET_X)+"px";
s.top=(e.pageY+this.OFFSET_Y)+"px";
var _b=Boolean(this.source.copyState(_1.isCopyKey(e)));
if(this.copy!=_b){
this._setCopyStatus(_b);
}
}
},onMouseUp:function(e){
if(this.avatar){
if(this.target&&this.canDropFlag){
var _c=Boolean(this.source.copyState(_1.isCopyKey(e))),_d=[this.source,this.nodes,_c,this.target,e];
_1.publish("/dnd/drop/before",_d);
_1.publish("/dnd/drop",_d);
}else{
_1.publish("/dnd/cancel");
}
this.stopDrag();
}
},onKeyDown:function(e){
if(this.avatar){
switch(e.keyCode){
case _1.keys.CTRL:
var _e=Boolean(this.source.copyState(true));
if(this.copy!=_e){
this._setCopyStatus(_e);
}
break;
case _1.keys.ESCAPE:
_1.publish("/dnd/cancel");
this.stopDrag();
break;
}
}
},onKeyUp:function(e){
if(this.avatar&&e.keyCode==_1.keys.CTRL){
var _f=Boolean(this.source.copyState(false));
if(this.copy!=_f){
this._setCopyStatus(_f);
}
}
},_setCopyStatus:function(_10){
this.copy=_10;
this.source._markDndStatus(this.copy);
this.updateAvatar();
_1.replaceClass(_1.body(),"dojoDnd"+(this.copy?"Copy":"Move"),"dojoDnd"+(this.copy?"Move":"Copy"));
}});
_1.dnd._manager=null;
_3.manager=_1.dnd.manager=function(){
if(!_1.dnd._manager){
_1.dnd._manager=new _1.dnd.Manager();
}
return _1.dnd._manager;
};
return _3;
});
