/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Mover",["../main","../Evented","../touch","./common","./autoscroll"],function(_1,_2,_3){
_1.declare("dojo.dnd.Mover",[_2],{constructor:function(_4,e,_5){
this.node=_1.byId(_4);
this.marginBox={l:e.pageX,t:e.pageY};
this.mouseButton=e.button;
var h=(this.host=_5),d=_4.ownerDocument;
this.events=[_1.connect(d,_3.move,this,"onFirstMove"),_1.connect(d,_3.move,this,"onMouseMove"),_1.connect(d,_3.release,this,"onMouseUp"),_1.connect(d,"ondragstart",_1.stopEvent),_1.connect(d.body,"onselectstart",_1.stopEvent)];
if(h&&h.onMoveStart){
h.onMoveStart(this);
}
},onMouseMove:function(e){
_1.dnd.autoScroll(e);
var m=this.marginBox;
this.host.onMove(this,{l:m.l+e.pageX,t:m.t+e.pageY},e);
_1.stopEvent(e);
},onMouseUp:function(e){
if(_1.isWebKit&&_1.isMac&&this.mouseButton==2?e.button==0:this.mouseButton==e.button){
this.destroy();
}
_1.stopEvent(e);
},onFirstMove:function(e){
var s=this.node.style,l,t,h=this.host;
switch(s.position){
case "relative":
case "absolute":
l=Math.round(parseFloat(s.left))||0;
t=Math.round(parseFloat(s.top))||0;
break;
default:
s.position="absolute";
var m=_1.marginBox(this.node);
var b=_1.doc.body;
var bs=_1.getComputedStyle(b);
var bm=_1._getMarginBox(b,bs);
var bc=_1._getContentBox(b,bs);
l=m.l-(bc.l-bm.l);
t=m.t-(bc.t-bm.t);
break;
}
this.marginBox.l=l-this.marginBox.l;
this.marginBox.t=t-this.marginBox.t;
if(h&&h.onFirstMove){
h.onFirstMove(this,e);
}
_1.disconnect(this.events.shift());
},destroy:function(){
_1.forEach(this.events,_1.disconnect);
var h=this.host;
if(h&&h.onMoveStop){
h.onMoveStop(this);
}
this.events=this.node=this.host=null;
}});
return _1.dnd.Mover;
});
