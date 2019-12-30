/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Mover",["../_base/array","../_base/declare","../_base/event","../_base/lang","../sniff","../_base/window","../dom","../dom-geometry","../dom-style","../Evented","../on","../touch","./common","./autoscroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d){
return _2("dojo.dnd.Mover",[_a],{constructor:function(_e,e,_f){
this.node=_7.byId(_e);
this.marginBox={l:e.pageX,t:e.pageY};
this.mouseButton=e.button;
var h=(this.host=_f),d=_e.ownerDocument;
this.events=[on(d,_b.move,_4.hitch(this,"onFirstMove")),on(d,_b.move,_4.hitch(this,"onMouseMove")),on(d,_b.release,_4.hitch(this,"onMouseUp")),on(d,"dragstart",_3.stop),on(d.body,"selectstart",_3.stop)];
_d.autoScrollStart(d);
if(h&&h.onMoveStart){
h.onMoveStart(this);
}
},onMouseMove:function(e){
_d.autoScroll(e);
var m=this.marginBox;
this.host.onMove(this,{l:m.l+e.pageX,t:m.t+e.pageY},e);
_3.stop(e);
},onMouseUp:function(e){
if(_5("webkit")&&_5("mac")&&this.mouseButton==2?e.button==0:this.mouseButton==e.button){
this.destroy();
}
_3.stop(e);
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
var m=_8.getMarginBox(this.node);
var b=_6.doc.body;
var bs=_9.getComputedStyle(b);
var bm=_8.getMarginBox(b,bs);
var bc=_8.getContentBox(b,bs);
l=m.l-(bc.l-bm.l);
t=m.t-(bc.t-bm.t);
break;
}
this.marginBox.l=l-this.marginBox.l;
this.marginBox.t=t-this.marginBox.t;
if(h&&h.onFirstMove){
h.onFirstMove(this,e);
}
this.events.shift().remove();
},destroy:function(){
_1.forEach(this.events,function(_10){
_10.remove();
});
var h=this.host;
if(h&&h.onMoveStop){
h.onMoveStop(this);
}
this.events=this.node=this.host=null;
}});
});
