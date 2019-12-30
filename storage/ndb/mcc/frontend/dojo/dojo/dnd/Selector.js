/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Selector",["../_base/array","../_base/declare","../_base/event","../_base/kernel","../_base/lang","../dom","../dom-construct","../mouse","../_base/NodeList","../on","../touch","./common","./Container"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c){
var _d=_2("dojo.dnd.Selector",_c,{constructor:function(_e,_f){
if(!_f){
_f={};
}
this.singular=_f.singular;
this.autoSync=_f.autoSync;
this.selection={};
this.anchor=null;
this.simpleSelection=false;
this.events.push(on(this.node,_a.press,_5.hitch(this,"onMouseDown")),on(this.node,_a.release,_5.hitch(this,"onMouseUp")));
},singular:false,getSelectedNodes:function(){
var t=new _9();
var e=_b._empty;
for(var i in this.selection){
if(i in e){
continue;
}
t.push(_6.byId(i));
}
return t;
},selectNone:function(){
return this._removeSelection()._removeAnchor();
},selectAll:function(){
this.forInItems(function(_10,id){
this._addItemClass(_6.byId(id),"Selected");
this.selection[id]=1;
},this);
return this._removeAnchor();
},deleteSelectedNodes:function(){
var e=_b._empty;
for(var i in this.selection){
if(i in e){
continue;
}
var n=_6.byId(i);
this.delItem(i);
_7.destroy(n);
}
this.anchor=null;
this.selection={};
return this;
},forInSelectedItems:function(f,o){
o=o||_4.global;
var s=this.selection,e=_b._empty;
for(var i in s){
if(i in e){
continue;
}
f.call(o,this.getItem(i),i,this);
}
},sync:function(){
_d.superclass.sync.call(this);
if(this.anchor){
if(!this.getItem(this.anchor.id)){
this.anchor=null;
}
}
var t=[],e=_b._empty;
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
},insertNodes:function(_11,_12,_13,_14){
var _15=this._normalizedCreator;
this._normalizedCreator=function(_16,_17){
var t=_15.call(this,_16,_17);
if(_11){
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
_d.superclass.insertNodes.call(this,_12,_13,_14);
this._normalizedCreator=_15;
return this;
},destroy:function(){
_d.superclass.destroy.call(this);
this.selection=this.anchor=null;
},onMouseDown:function(e){
if(this.autoSync){
this.sync();
}
if(!this.current){
return;
}
if(!this.singular&&!_b.getCopyKeyState(e)&&!e.shiftKey&&(this.current.id in this.selection)){
this.simpleSelection=true;
if(_8.isLeft(e)){
_3.stop(e);
}
return;
}
if(!this.singular&&e.shiftKey){
if(!_b.getCopyKeyState(e)){
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
var i=0,_18;
for(;i<c.length;++i){
_18=c[i];
if(_18==this.anchor||_18==this.current){
break;
}
}
for(++i;i<c.length;++i){
_18=c[i];
if(_18==this.anchor||_18==this.current){
break;
}
this._addItemClass(_18,"Selected");
this.selection[_18.id]=1;
}
this._addItemClass(this.current,"Selected");
this.selection[this.current.id]=1;
}
}
}else{
if(this.singular){
if(this.anchor==this.current){
if(_b.getCopyKeyState(e)){
this.selectNone();
}
}else{
this.selectNone();
this.anchor=this.current;
this._addItemClass(this.anchor,"Anchor");
this.selection[this.current.id]=1;
}
}else{
if(_b.getCopyKeyState(e)){
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
_3.stop(e);
},onMouseUp:function(){
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
},onMouseMove:function(){
this.simpleSelection=false;
},onOverEvent:function(){
this.onmousemoveEvent=on(this.node,_a.move,_5.hitch(this,"onMouseMove"));
},onOutEvent:function(){
if(this.onmousemoveEvent){
this.onmousemoveEvent.remove();
delete this.onmousemoveEvent;
}
},_removeSelection:function(){
var e=_b._empty;
for(var i in this.selection){
if(i in e){
continue;
}
var _19=_6.byId(i);
if(_19){
this._removeItemClass(_19,"Selected");
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
return _d;
});
