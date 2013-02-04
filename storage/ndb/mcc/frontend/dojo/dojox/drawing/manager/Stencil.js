//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.manager.Stencil");
(function(){
var _4,_5;
_3.drawing.manager.Stencil=_3.drawing.util.oo.declare(function(_6){
_4=_6.surface;
this.canvas=_6.canvas;
this.defaults=_3.drawing.defaults.copy();
this.undo=_6.undo;
this.mouse=_6.mouse;
this.keys=_6.keys;
this.anchors=_6.anchors;
this.stencils={};
this.selectedStencils={};
this._mouseHandle=this.mouse.register(this);
_2.connect(this.keys,"onArrow",this,"onArrow");
_2.connect(this.keys,"onEsc",this,"deselect");
_2.connect(this.keys,"onDelete",this,"onDelete");
},{_dragBegun:false,_wasDragged:false,_secondClick:false,_isBusy:false,setRecentStencil:function(_7){
this.recent=_7;
},getRecentStencil:function(){
return this.recent;
},register:function(_8){
if(_8.isText&&!_8.editMode&&_8.deleteEmptyCreate&&!_8.getText()){
console.warn("EMPTY CREATE DELETE",_8);
_8.destroy();
return false;
}
this.stencils[_8.id]=_8;
this.setRecentStencil(_8);
if(_8.execText){
if(_8._text&&!_8.editMode){
this.selectItem(_8);
}
_8.connect("execText",this,function(){
if(_8.isText&&_8.deleteEmptyModify&&!_8.getText()){
console.warn("EMPTY MOD DELETE",_8);
this.deleteItem(_8);
}else{
if(_8.selectOnExec){
this.selectItem(_8);
}
}
});
}
_8.connect("deselect",this,function(){
if(!this._isBusy&&this.isSelected(_8)){
this.deselectItem(_8);
}
});
_8.connect("select",this,function(){
if(!this._isBusy&&!this.isSelected(_8)){
this.selectItem(_8);
}
});
return _8;
},unregister:function(_9){
if(_9){
_9.selected&&this.onDeselect(_9);
delete this.stencils[_9.id];
}
},onArrow:function(_a){
if(this.hasSelected()){
this.saveThrottledState();
this.group.applyTransform({dx:_a.x,dy:_a.y});
}
},_throttleVrl:null,_throttle:false,throttleTime:400,_lastmxx:-1,_lastmxy:-1,saveMoveState:function(){
var mx=this.group.getTransform();
if(mx.dx==this._lastmxx&&mx.dy==this._lastmxy){
return;
}
this._lastmxx=mx.dx;
this._lastmxy=mx.dy;
this.undo.add({before:_2.hitch(this.group,"setTransform",mx)});
},saveThrottledState:function(){
clearTimeout(this._throttleVrl);
clearInterval(this._throttleVrl);
this._throttleVrl=setTimeout(_2.hitch(this,function(){
this._throttle=false;
this.saveMoveState();
}),this.throttleTime);
if(this._throttle){
return;
}
this._throttle=true;
this.saveMoveState();
},unDelete:function(_b){
for(var s in _b){
_b[s].render();
this.onSelect(_b[s]);
}
},onDelete:function(_c){
if(_c!==true){
this.undo.add({before:_2.hitch(this,"unDelete",this.selectedStencils),after:_2.hitch(this,"onDelete",true)});
}
this.withSelected(function(m){
this.anchors.remove(m);
var id=m.id;
m.destroy();
delete this.stencils[id];
});
this.selectedStencils={};
},deleteItem:function(_d){
if(this.hasSelected()){
var _e=[];
for(var m in this.selectedStencils){
if(this.selectedStencils.id==_d.id){
if(this.hasSelected()==1){
this.onDelete();
return;
}
}else{
_e.push(this.selectedStencils.id);
}
}
this.deselect();
this.selectItem(_d);
this.onDelete();
_2.forEach(_e,function(id){
this.selectItem(id);
},this);
}else{
this.selectItem(_d);
this.onDelete();
}
},removeAll:function(){
this.selectAll();
this._isBusy=true;
this.onDelete();
this.stencils={};
this._isBusy=false;
},setSelectionGroup:function(){
this.withSelected(function(m){
this.onDeselect(m,true);
});
if(this.group){
_4.remove(this.group);
this.group.removeShape();
}
this.group=_4.createGroup();
this.group.setTransform({dx:0,dy:0});
this.withSelected(function(m){
this.group.add(m.container);
m.select();
});
},setConstraint:function(){
var t=Infinity,l=Infinity;
this.withSelected(function(m){
var o=m.getBounds();
t=Math.min(o.y1,t);
l=Math.min(o.x1,l);
});
this.constrain={l:-l,t:-t};
},onDeselect:function(_f,_10){
if(!_10){
delete this.selectedStencils[_f.id];
}
this.anchors.remove(_f);
_4.add(_f.container);
_f.selected&&_f.deselect();
_f.applyTransform(this.group.getTransform());
},deselectItem:function(_11){
this.onDeselect(_11);
},deselect:function(){
this.withSelected(function(m){
this.onDeselect(m);
});
this._dragBegun=false;
this._wasDragged=false;
},onSelect:function(_12){
if(!_12){
console.error("null stencil is not selected:",this.stencils);
}
if(this.selectedStencils[_12.id]){
return;
}
this.selectedStencils[_12.id]=_12;
this.group.add(_12.container);
_12.select();
if(this.hasSelected()==1){
this.anchors.add(_12,this.group);
}
},selectAll:function(){
this._isBusy=true;
for(var m in this.stencils){
this.selectItem(m);
}
this._isBusy=false;
},selectItem:function(_13){
var id=typeof (_13)=="string"?_13:_13.id;
var _14=this.stencils[id];
this.setSelectionGroup();
this.onSelect(_14);
this.group.moveToFront();
this.setConstraint();
},onLabelDoubleClick:function(obj){
if(this.selectedStencils[obj.id]){
this.deselect();
}
},onStencilDoubleClick:function(obj){
if(this.selectedStencils[obj.id]){
if(this.selectedStencils[obj.id].edit){
var m=this.selectedStencils[obj.id];
m.editMode=true;
this.deselect();
m.edit();
}
}
},onAnchorUp:function(){
this.setConstraint();
},onStencilDown:function(obj,evt){
if(!this.stencils[obj.id]){
return;
}
this.setRecentStencil(this.stencils[obj.id]);
this._isBusy=true;
if(this.selectedStencils[obj.id]&&this.keys.meta){
if(_2.isMac&&this.keys.cmmd){
}
this.onDeselect(this.selectedStencils[obj.id]);
if(this.hasSelected()==1){
this.withSelected(function(m){
this.anchors.add(m,this.group);
});
}
this.group.moveToFront();
this.setConstraint();
return;
}else{
if(this.selectedStencils[obj.id]){
var mx=this.group.getTransform();
this._offx=obj.x-mx.dx;
this._offy=obj.y-mx.dy;
return;
}else{
if(!this.keys.meta){
this.deselect();
}else{
}
}
}
this.selectItem(obj.id);
mx=this.group.getTransform();
this._offx=obj.x-mx.dx;
this._offy=obj.y-mx.dx;
this.orgx=obj.x;
this.orgy=obj.y;
this._isBusy=false;
this.undo.add({before:function(){
},after:function(){
}});
},onLabelDown:function(obj,evt){
this.onStencilDown(obj,evt);
},onStencilUp:function(obj){
},onLabelUp:function(obj){
this.onStencilUp(obj);
},onStencilDrag:function(obj){
if(!this._dragBegun){
this.onBeginDrag(obj);
this._dragBegun=true;
}else{
this.saveThrottledState();
var x=obj.x-obj.last.x,y=obj.y-obj.last.y,c=this.constrain,mz=this.defaults.anchors.marginZero;
x=obj.x-this._offx;
y=obj.y-this._offy;
if(x<c.l+mz){
x=c.l+mz;
}
if(y<c.t+mz){
y=c.t+mz;
}
this.group.setTransform({dx:x,dy:y});
}
},onLabelDrag:function(obj){
this.onStencilDrag(obj);
},onDragEnd:function(obj){
this._dragBegun=false;
},onBeginDrag:function(obj){
this._wasDragged=true;
},onDown:function(obj){
this.deselect();
},onStencilOver:function(obj){
_2.style(obj.id,"cursor","move");
},onStencilOut:function(obj){
_2.style(obj.id,"cursor","crosshair");
},exporter:function(){
var _15=[];
for(var m in this.stencils){
this.stencils[m].enabled&&_15.push(this.stencils[m].exporter());
}
return _15;
},listStencils:function(){
return this.stencils;
},toSelected:function(_16){
var _17=Array.prototype.slice.call(arguments).splice(1);
for(var m in this.selectedStencils){
var _18=this.selectedStencils[m];
_18[_16].apply(_18,_17);
}
},withSelected:function(_19){
var f=_2.hitch(this,_19);
for(var m in this.selectedStencils){
f(this.selectedStencils[m]);
}
},withUnselected:function(_1a){
var f=_2.hitch(this,_1a);
for(var m in this.stencils){
!this.stencils[m].selected&&f(this.stencils[m]);
}
},withStencils:function(_1b){
var f=_2.hitch(this,_1b);
for(var m in this.stencils){
f(this.stencils[m]);
}
},hasSelected:function(){
var ln=0;
for(var m in this.selectedStencils){
ln++;
}
return ln;
},isSelected:function(_1c){
return !!this.selectedStencils[_1c.id];
}});
})();
});
