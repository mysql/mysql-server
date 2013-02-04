/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Selector",["../main","./common","./Container"],function(_1){
_1.declare("dojo.dnd.Selector",_1.dnd.Container,{constructor:function(_2,_3){
if(!_3){
_3={};
}
this.singular=_3.singular;
this.autoSync=_3.autoSync;
this.selection={};
this.anchor=null;
this.simpleSelection=false;
this.events.push(_1.connect(this.node,"onmousedown",this,"onMouseDown"),_1.connect(this.node,"onmouseup",this,"onMouseUp"));
},singular:false,getSelectedNodes:function(){
var t=new _1.NodeList();
var e=_1.dnd._empty;
for(var i in this.selection){
if(i in e){
continue;
}
t.push(_1.byId(i));
}
return t;
},selectNone:function(){
return this._removeSelection()._removeAnchor();
},selectAll:function(){
this.forInItems(function(_4,id){
this._addItemClass(_1.byId(id),"Selected");
this.selection[id]=1;
},this);
return this._removeAnchor();
},deleteSelectedNodes:function(){
var e=_1.dnd._empty;
for(var i in this.selection){
if(i in e){
continue;
}
var n=_1.byId(i);
this.delItem(i);
_1.destroy(n);
}
this.anchor=null;
this.selection={};
return this;
},forInSelectedItems:function(f,o){
o=o||_1.global;
var s=this.selection,e=_1.dnd._empty;
for(var i in s){
if(i in e){
continue;
}
f.call(o,this.getItem(i),i,this);
}
},sync:function(){
_1.dnd.Selector.superclass.sync.call(this);
if(this.anchor){
if(!this.getItem(this.anchor.id)){
this.anchor=null;
}
}
var t=[],e=_1.dnd._empty;
for(var i in this.selection){
if(i in e){
continue;
}
if(!this.getItem(i)){
t.push(i);
}
}
_1.forEach(t,function(i){
delete this.selection[i];
},this);
return this;
},insertNodes:function(_5,_6,_7,_8){
var _9=this._normalizedCreator;
this._normalizedCreator=function(_a,_b){
var t=_9.call(this,_a,_b);
if(_5){
if(!this.anchor){
this.anchor=t.node;
this._removeItemClass(t.node,"Selected");
this._addItemClass(this.anchor,"Anchor");
}else{
if(this.anchor!=t.node){
this._removeItemClass(t.node,"Anchor");
this._addItemClass(t.node,"Selected");
}
}
this.selection[t.node.id]=1;
}else{
this._removeItemClass(t.node,"Selected");
this._removeItemClass(t.node,"Anchor");
}
return t;
};
_1.dnd.Selector.superclass.insertNodes.call(this,_6,_7,_8);
this._normalizedCreator=_9;
return this;
},destroy:function(){
_1.dnd.Selector.superclass.destroy.call(this);
this.selection=this.anchor=null;
},onMouseDown:function(e){
if(this.autoSync){
this.sync();
}
if(!this.current){
return;
}
if(!this.singular&&!_1.isCopyKey(e)&&!e.shiftKey&&(this.current.id in this.selection)){
this.simpleSelection=true;
if(e.button===_1.mouseButtons.LEFT){
_1.stopEvent(e);
}
return;
}
if(!this.singular&&e.shiftKey){
if(!_1.isCopyKey(e)){
this._removeSelection();
}
var c=this.getAllNodes();
if(c.length){
if(!this.anchor){
this.anchor=c[0];
this._addItemClass(this.anchor,"Anchor");
}
this.selection[this.anchor.id]=1;
if(this.anchor!=this.current){
var i=0;
for(;i<c.length;++i){
var _c=c[i];
if(_c==this.anchor||_c==this.current){
break;
}
}
for(++i;i<c.length;++i){
var _c=c[i];
if(_c==this.anchor||_c==this.current){
break;
}
this._addItemClass(_c,"Selected");
this.selection[_c.id]=1;
}
this._addItemClass(this.current,"Selected");
this.selection[this.current.id]=1;
}
}
}else{
if(this.singular){
if(this.anchor==this.current){
if(_1.isCopyKey(e)){
this.selectNone();
}
}else{
this.selectNone();
this.anchor=this.current;
this._addItemClass(this.anchor,"Anchor");
this.selection[this.current.id]=1;
}
}else{
if(_1.isCopyKey(e)){
if(this.anchor==this.current){
delete this.selection[this.anchor.id];
this._removeAnchor();
}else{
if(this.current.id in this.selection){
this._removeItemClass(this.current,"Selected");
delete this.selection[this.current.id];
}else{
if(this.anchor){
this._removeItemClass(this.anchor,"Anchor");
this._addItemClass(this.anchor,"Selected");
}
this.anchor=this.current;
this._addItemClass(this.current,"Anchor");
this.selection[this.current.id]=1;
}
}
}else{
if(!(this.current.id in this.selection)){
this.selectNone();
this.anchor=this.current;
this._addItemClass(this.current,"Anchor");
this.selection[this.current.id]=1;
}
}
}
}
_1.stopEvent(e);
},onMouseUp:function(e){
if(!this.simpleSelection){
return;
}
this.simpleSelection=false;
this.selectNone();
if(this.current){
this.anchor=this.current;
this._addItemClass(this.anchor,"Anchor");
this.selection[this.current.id]=1;
}
},onMouseMove:function(e){
this.simpleSelection=false;
},onOverEvent:function(){
this.onmousemoveEvent=_1.connect(this.node,"onmousemove",this,"onMouseMove");
},onOutEvent:function(){
_1.disconnect(this.onmousemoveEvent);
delete this.onmousemoveEvent;
},_removeSelection:function(){
var e=_1.dnd._empty;
for(var i in this.selection){
if(i in e){
continue;
}
var _d=_1.byId(i);
if(_d){
this._removeItemClass(_d,"Selected");
}
}
this.selection={};
return this;
},_removeAnchor:function(){
if(this.anchor){
this._removeItemClass(this.anchor,"Anchor");
this.anchor=null;
}
return this;
}});
return _1.dnd.Selector;
});
