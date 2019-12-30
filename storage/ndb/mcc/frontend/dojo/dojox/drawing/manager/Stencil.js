//>>built
define("dojox/drawing/manager/Stencil",["dojo","../util/oo","../defaults"],function(_1,oo,_2){
var _3,_4;
return oo.declare(function(_5){
_3=_5.surface;
this.canvas=_5.canvas;
this.undo=_5.undo;
this.mouse=_5.mouse;
this.keys=_5.keys;
this.anchors=_5.anchors;
this.stencils={};
this.selectedStencils={};
this._mouseHandle=this.mouse.register(this);
_1.connect(this.keys,"onArrow",this,"onArrow");
_1.connect(this.keys,"onEsc",this,"deselect");
_1.connect(this.keys,"onDelete",this,"onDelete");
},{_dragBegun:false,_wasDragged:false,_secondClick:false,_isBusy:false,setRecentStencil:function(_6){
this.recent=_6;
},getRecentStencil:function(){
return this.recent;
},register:function(_7){
if(_7.isText&&!_7.editMode&&_7.deleteEmptyCreate&&!_7.getText()){
console.warn("EMPTY CREATE DELETE",_7);
_7.destroy();
return false;
}
this.stencils[_7.id]=_7;
this.setRecentStencil(_7);
if(_7.execText){
if(_7._text&&!_7.editMode){
this.selectItem(_7);
}
_7.connect("execText",this,function(){
if(_7.isText&&_7.deleteEmptyModify&&!_7.getText()){
console.warn("EMPTY MOD DELETE",_7);
this.deleteItem(_7);
}else{
if(_7.selectOnExec){
this.selectItem(_7);
}
}
});
}
_7.connect("deselect",this,function(){
if(!this._isBusy&&this.isSelected(_7)){
this.deselectItem(_7);
}
});
_7.connect("select",this,function(){
if(!this._isBusy&&!this.isSelected(_7)){
this.selectItem(_7);
}
});
return _7;
},unregister:function(_8){
if(_8){
_8.selected&&this.onDeselect(_8);
delete this.stencils[_8.id];
}
},onArrow:function(_9){
if(this.hasSelected()){
this.saveThrottledState();
this.group.applyTransform({dx:_9.x,dy:_9.y});
}
},_throttleVrl:null,_throttle:false,throttleTime:400,_lastmxx:-1,_lastmxy:-1,saveMoveState:function(){
var mx=this.group.getTransform();
if(mx.dx==this._lastmxx&&mx.dy==this._lastmxy){
return;
}
this._lastmxx=mx.dx;
this._lastmxy=mx.dy;
this.undo.add({before:_1.hitch(this.group,"setTransform",mx)});
},saveThrottledState:function(){
clearTimeout(this._throttleVrl);
clearInterval(this._throttleVrl);
this._throttleVrl=setTimeout(_1.hitch(this,function(){
this._throttle=false;
this.saveMoveState();
}),this.throttleTime);
if(this._throttle){
return;
}
this._throttle=true;
this.saveMoveState();
},unDelete:function(_a){
for(var s in _a){
_a[s].render();
this.onSelect(_a[s]);
}
},onDelete:function(_b){
if(_b!==true){
this.undo.add({before:_1.hitch(this,"unDelete",this.selectedStencils),after:_1.hitch(this,"onDelete",true)});
}
this.withSelected(function(m){
this.anchors.remove(m);
var id=m.id;
m.destroy();
delete this.stencils[id];
});
this.selectedStencils={};
},deleteItem:function(_c){
if(this.hasSelected()){
var _d=[];
for(var m in this.selectedStencils){
if(this.selectedStencils.id==_c.id){
if(this.hasSelected()==1){
this.onDelete();
return;
}
}else{
_d.push(this.selectedStencils.id);
}
}
this.deselect();
this.selectItem(_c);
this.onDelete();
_1.forEach(_d,function(id){
this.selectItem(id);
},this);
}else{
this.selectItem(_c);
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
_3.remove(this.group);
this.group.removeShape();
}
this.group=_3.createGroup();
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
},onDeselect:function(_e,_f){
if(!_f){
delete this.selectedStencils[_e.id];
}
this.anchors.remove(_e);
_3.add(_e.container);
_e.selected&&_e.deselect();
_e.applyTransform(this.group.getTransform());
},deselectItem:function(_10){
this.onDeselect(_10);
},deselect:function(){
this.withSelected(function(m){
this.onDeselect(m);
});
this._dragBegun=false;
this._wasDragged=false;
},onSelect:function(_11){
if(!_11){
console.error("null stencil is not selected:",this.stencils);
}
if(this.selectedStencils[_11.id]){
return;
}
this.selectedStencils[_11.id]=_11;
this.group.add(_11.container);
_11.select();
if(this.hasSelected()==1){
this.anchors.add(_11,this.group);
}
},selectAll:function(){
this._isBusy=true;
for(var m in this.stencils){
this.selectItem(m);
}
this._isBusy=false;
},selectItem:function(_12){
var id=typeof (_12)=="string"?_12:_12.id;
var _13=this.stencils[id];
this.setSelectionGroup();
this.onSelect(_13);
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
if(_1.isMac&&this.keys.cmmd){
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
var x=obj.x-obj.last.x,y=obj.y-obj.last.y,c=this.constrain,mz=_2.anchors.marginZero;
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
_1.style(obj.id,"cursor","move");
},onStencilOut:function(obj){
_1.style(obj.id,"cursor","crosshair");
},exporter:function(){
var _14=[];
for(var m in this.stencils){
this.stencils[m].enabled&&_14.push(this.stencils[m].exporter());
}
return _14;
},listStencils:function(){
return this.stencils;
},toSelected:function(_15){
var _16=Array.prototype.slice.call(arguments).splice(1);
for(var m in this.selectedStencils){
var _17=this.selectedStencils[m];
_17[_15].apply(_17,_16);
}
},withSelected:function(_18){
var f=_1.hitch(this,_18);
for(var m in this.selectedStencils){
f(this.selectedStencils[m]);
}
},withUnselected:function(_19){
var f=_1.hitch(this,_19);
for(var m in this.stencils){
!this.stencils[m].selected&&f(this.stencils[m]);
}
},withStencils:function(_1a){
var f=_1.hitch(this,_1a);
for(var m in this.stencils){
f(this.stencils[m]);
}
},hasSelected:function(){
var ln=0;
for(var m in this.selectedStencils){
ln++;
}
return ln;
},isSelected:function(_1b){
return !!this.selectedStencils[_1b.id];
}});
});
