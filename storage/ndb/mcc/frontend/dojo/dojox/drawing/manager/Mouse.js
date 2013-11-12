//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.manager.Mouse");
_3.drawing.manager.Mouse=_3.drawing.util.oo.declare(function(_4){
this.util=_4.util;
this.keys=_4.keys;
this.id=_4.id||this.util.uid("mouse");
this.currentNodeId="";
this.registered={};
},{doublClickSpeed:400,_lastx:0,_lasty:0,__reg:0,_downOnCanvas:false,init:function(_5){
this.container=_5;
this.setCanvas();
var c;
var _6=false;
_2.connect(this.container,"rightclick",this,function(_7){
console.warn("RIGHTCLICK");
});
_2.connect(document.body,"mousedown",this,function(_8){
});
_2.connect(this.container,"mousedown",this,function(_9){
this.down(_9);
if(_9.button!=_2.mouseButtons.RIGHT){
_6=true;
c=_2.connect(document,"mousemove",this,"drag");
}
});
_2.connect(document,"mouseup",this,function(_a){
_2.disconnect(c);
_6=false;
this.up(_a);
});
_2.connect(document,"mousemove",this,function(_b){
if(!_6){
this.move(_b);
}
});
_2.connect(this.keys,"onEsc",this,function(_c){
this._dragged=false;
});
},setCanvas:function(){
var _d=_2.coords(this.container.parentNode);
this.origin=_2.clone(_d);
},scrollOffset:function(){
return {top:this.container.parentNode.scrollTop,left:this.container.parentNode.scrollLeft};
},resize:function(_e,_f){
if(this.origin){
this.origin.w=_e;
this.origin.h=_f;
}
},register:function(_10){
var _11=_10.id||"reg_"+(this.__reg++);
if(!this.registered[_11]){
this.registered[_11]=_10;
}
return _11;
},unregister:function(_12){
if(!this.registered[_12]){
return;
}
delete this.registered[_12];
},_broadcastEvent:function(_13,obj){
for(var nm in this.registered){
if(this.registered[nm][_13]){
this.registered[nm][_13](obj);
}
}
},onDown:function(obj){
this._broadcastEvent(this.eventName("down"),obj);
},onDrag:function(obj){
var nm=this.eventName("drag");
if(this._selected&&nm=="onDrag"){
nm="onStencilDrag";
}
this._broadcastEvent(nm,obj);
},onMove:function(obj){
this._broadcastEvent("onMove",obj);
},overName:function(obj,evt){
var nm=obj.id.split(".");
evt=evt.charAt(0).toUpperCase()+evt.substring(1);
if(nm[0]=="dojox"&&(_3.drawing.defaults.clickable||!_3.drawing.defaults.clickMode)){
return "onStencil"+evt;
}else{
return "on"+evt;
}
},onOver:function(obj){
this._broadcastEvent(this.overName(obj,"over"),obj);
},onOut:function(obj){
this._broadcastEvent(this.overName(obj,"out"),obj);
},onUp:function(obj){
var nm=this.eventName("up");
if(nm=="onStencilUp"){
this._selected=true;
}else{
if(this._selected&&nm=="onUp"){
nm="onStencilUp";
this._selected=false;
}
}
this._broadcastEvent(nm,obj);
if(_3.gfx.renderer=="silverlight"){
return;
}
this._clickTime=new Date().getTime();
if(this._lastClickTime){
if(this._clickTime-this._lastClickTime<this.doublClickSpeed){
var dnm=this.eventName("doubleClick");
console.warn("DOUBLE CLICK",dnm,obj);
this._broadcastEvent(dnm,obj);
}else{
}
}
this._lastClickTime=this._clickTime;
},zoom:1,setZoom:function(_14){
this.zoom=1/_14;
},setEventMode:function(_15){
this.mode=_15?"on"+_15.charAt(0).toUpperCase()+_15.substring(1):"";
},eventName:function(_16){
_16=_16.charAt(0).toUpperCase()+_16.substring(1);
if(this.mode){
if(this.mode=="onPathEdit"){
return "on"+_16;
}
if(this.mode=="onUI"){
}
return this.mode+_16;
}else{
if(!_3.drawing.defaults.clickable&&_3.drawing.defaults.clickMode){
return "on"+_16;
}
var dt=!this.drawingType||this.drawingType=="surface"||this.drawingType=="canvas"?"":this.drawingType;
var t=!dt?"":dt.charAt(0).toUpperCase()+dt.substring(1);
return "on"+t+_16;
}
},up:function(evt){
this.onUp(this.create(evt));
},down:function(evt){
this._downOnCanvas=true;
var sc=this.scrollOffset();
var dim=this._getXY(evt);
this._lastpagex=dim.x;
this._lastpagey=dim.y;
var o=this.origin;
var x=dim.x-o.x+sc.left;
var y=dim.y-o.y+sc.top;
var _17=x>=0&&y>=0&&x<=o.w&&y<=o.h;
x*=this.zoom;
y*=this.zoom;
o.startx=x;
o.starty=y;
this._lastx=x;
this._lasty=y;
this.drawingType=this.util.attr(evt,"drawingType")||"";
var id=this._getId(evt);
if(evt.button==_2.mouseButtons.RIGHT&&this.id=="mse"){
}else{
evt.preventDefault();
_2.stopEvent(evt);
}
this.onDown({mid:this.id,x:x,y:y,pageX:dim.x,pageY:dim.y,withinCanvas:_17,id:id});
},over:function(obj){
this.onOver(obj);
},out:function(obj){
this.onOut(obj);
},move:function(evt){
var obj=this.create(evt);
if(this.id=="MUI"){
}
if(obj.id!=this.currentNodeId){
var _18={};
for(var nm in obj){
_18[nm]=obj[nm];
}
_18.id=this.currentNodeId;
this.currentNodeId&&this.out(_18);
obj.id&&this.over(obj);
this.currentNodeId=obj.id;
}
this.onMove(obj);
},drag:function(evt){
this.onDrag(this.create(evt,true));
},create:function(evt,_19){
var sc=this.scrollOffset();
var dim=this._getXY(evt);
var _1a=dim.x;
var _1b=dim.y;
var o=this.origin;
var x=dim.x-o.x+sc.left;
var y=dim.y-o.y+sc.top;
var _1c=x>=0&&y>=0&&x<=o.w&&y<=o.h;
x*=this.zoom;
y*=this.zoom;
var id=_1c?this._getId(evt,_19):"";
var ret={mid:this.id,x:x,y:y,pageX:dim.x,pageY:dim.y,page:{x:dim.x,y:dim.y},orgX:o.x,orgY:o.y,last:{x:this._lastx,y:this._lasty},start:{x:this.origin.startx,y:this.origin.starty},move:{x:_1a-this._lastpagex,y:_1b-this._lastpagey},scroll:sc,id:id,withinCanvas:_1c};
this._lastx=x;
this._lasty=y;
this._lastpagex=_1a;
this._lastpagey=_1b;
_2.stopEvent(evt);
return ret;
},_getId:function(evt,_1d){
return this.util.attr(evt,"id",null,_1d);
},_getXY:function(evt){
return {x:evt.pageX,y:evt.pageY};
},setCursor:function(_1e,_1f){
if(!_1f){
_2.style(this.container,"cursor",_1e);
}else{
_2.style(_1f,"cursor",_1e);
}
}});
});
