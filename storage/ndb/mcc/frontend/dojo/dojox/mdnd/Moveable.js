//>>built
define("dojox/mdnd/Moveable",["dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/sniff","dojo/touch","dojo/dom","dojo/dom-geometry","dojo/dom-style","./AutoScroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dojox.mdnd.Moveable",null,{handle:null,skip:true,dragDistance:3,constructor:function(_b,_c){
this.node=_7.byId(_c);
this.d=this.node.ownerDocument;
if(!_b){
_b={};
}
this.handle=_b.handle?_7.byId(_b.handle):null;
if(!this.handle){
this.handle=this.node;
}
this.skip=_b.skip;
this.events=[_3.connect(this.handle,_6.press,this,"onMouseDown")];
if(_a.autoScroll){
this.autoScroll=_a.autoScroll;
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
var _d=(e.which||e.button)==1;
if(!_d){
return;
}
if(this.skip&&this.isFormElement(e)){
return;
}
if(this.autoScroll){
this.autoScroll.setAutoScrollNode(this.node);
this.autoScroll.setAutoScrollMaxPage();
}
this.events.push(_3.connect(this.d,_6.release,this,"onMouseUp"));
this.events.push(_3.connect(this.d,_6.move,this,"onFirstMove"));
this._selectStart=_3.connect(dojo.body(),"onselectstart",_4.stop);
this._firstX=e.clientX;
this._firstY=e.clientY;
_4.stop(e);
},onFirstMove:function(e){
_4.stop(e);
var d=(this._firstX-e.clientX)*(this._firstX-e.clientX)+(this._firstY-e.clientY)*(this._firstY-e.clientY);
if(d>this.dragDistance*this.dragDistance){
this._isDragging=true;
_3.disconnect(this.events.pop());
_9.set(this.node,"width",_8.getContentBox(this.node).w+"px");
this.initOffsetDrag(e);
this.events.push(_3.connect(this.d,_6.move,this,"onMove"));
}
},initOffsetDrag:function(e){
this.offsetDrag={"l":e.pageX,"t":e.pageY};
var s=this.node.style;
var _e=_8.position(this.node,true);
this.offsetDrag.l=_e.x-this.offsetDrag.l;
this.offsetDrag.t=_e.y-this.offsetDrag.t;
var _f={"x":_e.x,"y":_e.y};
this.size={"w":_e.w,"h":_e.h};
this.onDragStart(this.node,_f,this.size);
},onMove:function(e){
_4.stop(e);
if(_5("ie")==8&&new Date()-this.date<20){
return;
}
if(this.autoScroll){
this.autoScroll.checkAutoScroll(e);
}
var _10={"x":this.offsetDrag.l+e.pageX,"y":this.offsetDrag.t+e.pageY};
var s=this.node.style;
s.left=_10.x+"px";
s.top=_10.y+"px";
this.onDrag(this.node,_10,this.size,{"x":e.pageX,"y":e.pageY});
if(_5("ie")==8){
this.date=new Date();
}
},onMouseUp:function(e){
if(this._isDragging){
_4.stop(e);
this._isDragging=false;
if(this.autoScroll){
this.autoScroll.stopAutoScroll();
}
delete this.onMove;
this.onDragEnd(this.node);
this.node.focus();
}
_3.disconnect(this.events.pop());
_3.disconnect(this.events.pop());
_3.disconnect(this._selectStart);
this._selectStart=null;
},onDragStart:function(_11,_12,_13){
},onDragEnd:function(_14){
},onDrag:function(_15,_16,_17,_18){
},destroy:function(){
_2.forEach(this.events,_3.disconnect);
this.events=this.node=null;
}});
});
