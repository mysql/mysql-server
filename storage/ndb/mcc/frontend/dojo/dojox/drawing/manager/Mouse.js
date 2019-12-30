//>>built
define("dojox/drawing/manager/Mouse",["dojo","../util/oo","../defaults"],function(_1,oo,_2){
return oo.declare(function(_3){
this.util=_3.util;
this.keys=_3.keys;
this.id=_3.id||this.util.uid("mouse");
this.currentNodeId="";
this.registered={};
},{doublClickSpeed:400,_lastx:0,_lasty:0,__reg:0,_downOnCanvas:false,init:function(_4){
this.container=_4;
this.setCanvas();
var c;
var _5=false;
_1.connect(this.container,"rightclick",this,function(_6){
console.warn("RIGHTCLICK");
});
_1.connect(document.body,"mousedown",this,function(_7){
});
_1.connect(this.container,"mousedown",this,function(_8){
this.down(_8);
if(_8.button!=_1.mouseButtons.RIGHT){
_5=true;
c=_1.connect(document,"mousemove",this,"drag");
}
});
_1.connect(document,"mouseup",this,function(_9){
_1.disconnect(c);
_5=false;
this.up(_9);
});
_1.connect(document,"mousemove",this,function(_a){
if(!_5){
this.move(_a);
}
});
_1.connect(this.keys,"onEsc",this,function(_b){
this._dragged=false;
});
},setCanvas:function(){
var _c=_1.coords(this.container.parentNode);
this.origin=_1.clone(_c);
},scrollOffset:function(){
return {top:this.container.parentNode.scrollTop,left:this.container.parentNode.scrollLeft};
},resize:function(_d,_e){
if(this.origin){
this.origin.w=_d;
this.origin.h=_e;
}
},register:function(_f){
var _10=_f.id||"reg_"+(this.__reg++);
if(!this.registered[_10]){
this.registered[_10]=_f;
}
return _10;
},unregister:function(_11){
if(!this.registered[_11]){
return;
}
delete this.registered[_11];
},_broadcastEvent:function(_12,obj){
for(var nm in this.registered){
if(this.registered[nm][_12]){
this.registered[nm][_12](obj);
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
if(nm[0]=="dojox"&&(_2.clickable||!_2.clickMode)){
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
if(dojox.gfx.renderer=="silverlight"){
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
},zoom:1,setZoom:function(_13){
this.zoom=1/_13;
},setEventMode:function(_14){
this.mode=_14?"on"+_14.charAt(0).toUpperCase()+_14.substring(1):"";
},eventName:function(_15){
_15=_15.charAt(0).toUpperCase()+_15.substring(1);
if(this.mode){
if(this.mode=="onPathEdit"){
return "on"+_15;
}
if(this.mode=="onUI"){
}
return this.mode+_15;
}else{
if(!_2.clickable&&_2.clickMode){
return "on"+_15;
}
var dt=!this.drawingType||this.drawingType=="surface"||this.drawingType=="canvas"?"":this.drawingType;
var t=!dt?"":dt.charAt(0).toUpperCase()+dt.substring(1);
return "on"+t+_15;
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
var _16=x>=0&&y>=0&&x<=o.w&&y<=o.h;
x*=this.zoom;
y*=this.zoom;
o.startx=x;
o.starty=y;
this._lastx=x;
this._lasty=y;
this.drawingType=this.util.attr(evt,"drawingType")||"";
var id=this._getId(evt);
if(evt.button==_1.mouseButtons.RIGHT&&this.id=="mse"){
}else{
evt.preventDefault();
_1.stopEvent(evt);
}
this.onDown({mid:this.id,x:x,y:y,pageX:dim.x,pageY:dim.y,withinCanvas:_16,id:id});
},over:function(obj){
this.onOver(obj);
},out:function(obj){
this.onOut(obj);
},move:function(evt){
var obj=this.create(evt);
if(this.id=="MUI"){
}
if(obj.id!=this.currentNodeId){
var _17={};
for(var nm in obj){
_17[nm]=obj[nm];
}
_17.id=this.currentNodeId;
this.currentNodeId&&this.out(_17);
obj.id&&this.over(obj);
this.currentNodeId=obj.id;
}
this.onMove(obj);
},drag:function(evt){
this.onDrag(this.create(evt,true));
},create:function(evt,_18){
var sc=this.scrollOffset();
var dim=this._getXY(evt);
var _19=dim.x;
var _1a=dim.y;
var o=this.origin;
var x=dim.x-o.x+sc.left;
var y=dim.y-o.y+sc.top;
var _1b=x>=0&&y>=0&&x<=o.w&&y<=o.h;
x*=this.zoom;
y*=this.zoom;
var id=_1b?this._getId(evt,_18):"";
var ret={mid:this.id,x:x,y:y,pageX:dim.x,pageY:dim.y,page:{x:dim.x,y:dim.y},orgX:o.x,orgY:o.y,last:{x:this._lastx,y:this._lasty},start:{x:this.origin.startx,y:this.origin.starty},move:{x:_19-this._lastpagex,y:_1a-this._lastpagey},scroll:sc,id:id,withinCanvas:_1b};
this._lastx=x;
this._lasty=y;
this._lastpagex=_19;
this._lastpagey=_1a;
_1.stopEvent(evt);
return ret;
},_getId:function(evt,_1c){
return this.util.attr(evt,"id",null,_1c);
},_getXY:function(evt){
return {x:evt.pageX,y:evt.pageY};
},setCursor:function(_1d,_1e){
if(!_1e){
_1.style(this.container,"cursor",_1d);
}else{
_1.style(_1e,"cursor",_1d);
}
}});
});
