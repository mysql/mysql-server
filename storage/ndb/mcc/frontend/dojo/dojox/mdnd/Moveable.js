//>>built
define("dojox/mdnd/Moveable",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/sniff","dojo/dom","dojo/dom-geometry","dojo/dom-style","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mdnd.Moveable",null,{handle:null,skip:true,dragDistance:3,constructor:function(_a,_b){
this.node=_7.byId(_b);
this.d=this.node.ownerDocument;
if(!_a){
_a={};
}
this.handle=_a.handle?_7.byId(_a.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.skip=_a.skip;
this.events=[_4.connect(this.handle,"onmousedown",this,"onMouseDown")];
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
var _c=(e.which||e.button)==1;
if(!_c){
return;
}
if(this.skip&&this.isFormElement(e)){
return;
}
if(this.autoScroll){
this.autoScroll.setAutoScrollNode(this.node);
this.autoScroll.setAutoScrollMaxPage();
}
this.events.push(_4.connect(this.d,"onmouseup",this,"onMouseUp"));
this.events.push(_4.connect(this.d,"onmousemove",this,"onFirstMove"));
this._selectStart=_4.connect(_1.body(),"onselectstart",_5.stop);
this._firstX=e.clientX;
this._firstY=e.clientY;
_5.stop(e);
},onFirstMove:function(e){
_5.stop(e);
var d=(this._firstX-e.clientX)*(this._firstX-e.clientX)+(this._firstY-e.clientY)*(this._firstY-e.clientY);
if(d>this.dragDistance*this.dragDistance){
this._isDragging=true;
_4.disconnect(this.events.pop());
_9.set(this.node,"width",_8.getContentBox(this.node).w+"px");
this.initOffsetDrag(e);
this.events.push(_4.connect(this.d,"onmousemove",this,"onMove"));
}
},initOffsetDrag:function(e){
this.offsetDrag={"l":e.pageX,"t":e.pageY};
var s=this.node.style;
var _d=_8.position(this.node,true);
this.offsetDrag.l=_d.x-this.offsetDrag.l;
this.offsetDrag.t=_d.y-this.offsetDrag.t;
var _e={"x":_d.x,"y":_d.y};
this.size={"w":_d.w,"h":_d.h};
this.onDragStart(this.node,_e,this.size);
},onMove:function(e){
_5.stop(e);
if(_6("ie")==8&&new Date()-this.date<20){
return;
}
if(this.autoScroll){
this.autoScroll.checkAutoScroll(e);
}
var _f={"x":this.offsetDrag.l+e.pageX,"y":this.offsetDrag.t+e.pageY};
var s=this.node.style;
s.left=_f.x+"px";
s.top=_f.y+"px";
this.onDrag(this.node,_f,this.size,{"x":e.pageX,"y":e.pageY});
if(_6("ie")==8){
this.date=new Date();
}
},onMouseUp:function(e){
if(this._isDragging){
_5.stop(e);
this._isDragging=false;
if(this.autoScroll){
this.autoScroll.stopAutoScroll();
}
delete this.onMove;
this.onDragEnd(this.node);
this.node.focus();
}
_4.disconnect(this.events.pop());
_4.disconnect(this.events.pop());
},onDragStart:function(_10,_11,_12){
},onDragEnd:function(_13){
},onDrag:function(_14,_15,_16,_17){
},destroy:function(){
_3.forEach(this.events,_4.disconnect);
this.events=this.node=null;
}});
});
