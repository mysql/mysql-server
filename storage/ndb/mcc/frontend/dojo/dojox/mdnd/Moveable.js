//>>built
define("dojox/mdnd/Moveable",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/html","dojo/_base/sniff","dojo/_base/window"],function(_1){
return _1.declare("dojox.mdnd.Moveable",null,{handle:null,skip:true,dragDistance:3,constructor:function(_2,_3){
this.node=_1.byId(_3);
this.d=this.node.ownerDocument;
if(!_2){
_2={};
}
this.handle=_2.handle?_1.byId(_2.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.skip=_2.skip;
this.events=[_1.connect(this.handle,"onmousedown",this,"onMouseDown")];
if(dojox.mdnd.autoScroll){
this.autoScroll=dojox.mdnd.autoScroll;
}
},isFormElement:function(e){
var t=e.target;
if(t.nodeType==3){
t=t.parentNode;
}
return " a button textarea input select option ".indexOf(" "+t.tagName.toLowerCase()+" ")>=0;
},onMouseDown:function(e){
if(this._isDragging){
return;
}
var _4=(e.which||e.button)==1;
if(!_4){
return;
}
if(this.skip&&this.isFormElement(e)){
return;
}
if(this.autoScroll){
this.autoScroll.setAutoScrollNode(this.node);
this.autoScroll.setAutoScrollMaxPage();
}
this.events.push(_1.connect(this.d,"onmouseup",this,"onMouseUp"));
this.events.push(_1.connect(this.d,"onmousemove",this,"onFirstMove"));
this._selectStart=_1.connect(_1.body(),"onselectstart",_1.stopEvent);
this._firstX=e.clientX;
this._firstY=e.clientY;
_1.stopEvent(e);
},onFirstMove:function(e){
_1.stopEvent(e);
var d=(this._firstX-e.clientX)*(this._firstX-e.clientX)+(this._firstY-e.clientY)*(this._firstY-e.clientY);
if(d>this.dragDistance*this.dragDistance){
this._isDragging=true;
_1.disconnect(this.events.pop());
_1.style(this.node,"width",_1.contentBox(this.node).w+"px");
this.initOffsetDrag(e);
this.events.push(_1.connect(this.d,"onmousemove",this,"onMove"));
}
},initOffsetDrag:function(e){
this.offsetDrag={"l":e.pageX,"t":e.pageY};
var s=this.node.style;
var _5=_1.position(this.node,true);
this.offsetDrag.l=_5.x-this.offsetDrag.l;
this.offsetDrag.t=_5.y-this.offsetDrag.t;
var _6={"x":_5.x,"y":_5.y};
this.size={"w":_5.w,"h":_5.h};
this.onDragStart(this.node,_6,this.size);
},onMove:function(e){
_1.stopEvent(e);
if(_1.isIE==8&&new Date()-this.date<20){
return;
}
if(this.autoScroll){
this.autoScroll.checkAutoScroll(e);
}
var _7={"x":this.offsetDrag.l+e.pageX,"y":this.offsetDrag.t+e.pageY};
var s=this.node.style;
s.left=_7.x+"px";
s.top=_7.y+"px";
this.onDrag(this.node,_7,this.size,{"x":e.pageX,"y":e.pageY});
if(_1.isIE==8){
this.date=new Date();
}
},onMouseUp:function(e){
if(this._isDragging){
_1.stopEvent(e);
this._isDragging=false;
if(this.autoScroll){
this.autoScroll.stopAutoScroll();
}
delete this.onMove;
this.onDragEnd(this.node);
this.node.focus();
}
_1.disconnect(this.events.pop());
_1.disconnect(this.events.pop());
},onDragStart:function(_8,_9,_a){
},onDragEnd:function(_b){
},onDrag:function(_c,_d,_e,_f){
},destroy:function(){
_1.forEach(this.events,_1.disconnect);
this.events=this.node=null;
}});
});
