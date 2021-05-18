//>>built
define("dijit/tree/_dndSelector",["dojo/_base/array","dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/dnd/common","dojo/dom","dojo/mouse","dojo/on","dojo/touch","../a11yclick","./_dndContainer"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a){
return _2("dijit.tree._dndSelector",_a,{constructor:function(){
this.selection={};
this.anchor=null;
this.events.push(on(this.tree.domNode,_8.press,_4.hitch(this,"onMouseDown")),on(this.tree.domNode,_8.release,_4.hitch(this,"onMouseUp")),on(this.tree.domNode,_8.move,_4.hitch(this,"onMouseMove")),on(this.tree.domNode,_9.press,_4.hitch(this,"onClickPress")),on(this.tree.domNode,_9.release,_4.hitch(this,"onClickRelease")));
},singular:false,getSelectedTreeNodes:function(){
var _b=[],_c=this.selection;
for(var i in _c){
_b.push(_c[i]);
}
return _b;
},selectNone:function(){
this.setSelection([]);
return this;
},destroy:function(){
this.inherited(arguments);
this.selection=this.anchor=null;
},addTreeNode:function(_d,_e){
this.setSelection(this.getSelectedTreeNodes().concat([_d]));
if(_e){
this.anchor=_d;
}
return _d;
},removeTreeNode:function(_f){
var _10=_1.filter(this.getSelectedTreeNodes(),function(_11){
return !_6.isDescendant(_11.domNode,_f.domNode);
});
this.setSelection(_10);
return _f;
},isTreeNodeSelected:function(_12){
return _12.id&&!!this.selection[_12.id];
},setSelection:function(_13){
var _14=this.getSelectedTreeNodes();
_1.forEach(this._setDifference(_14,_13),_4.hitch(this,function(_15){
_15.setSelected(false);
if(this.anchor==_15){
delete this.anchor;
}
delete this.selection[_15.id];
}));
_1.forEach(this._setDifference(_13,_14),_4.hitch(this,function(_16){
_16.setSelected(true);
this.selection[_16.id]=_16;
}));
this._updateSelectionProperties();
},_setDifference:function(xs,ys){
_1.forEach(ys,function(y){
y.__exclude__=true;
});
var ret=_1.filter(xs,function(x){
return !x.__exclude__;
});
_1.forEach(ys,function(y){
delete y["__exclude__"];
});
return ret;
},_updateSelectionProperties:function(){
var _17=this.getSelectedTreeNodes();
var _18=[],_19=[];
_1.forEach(_17,function(_1a){
var ary=_1a.getTreePath();
_19.push(_1a);
_18.push(ary);
},this);
var _1b=_1.map(_19,function(_1c){
return _1c.item;
});
this.tree._set("paths",_18);
this.tree._set("path",_18[0]||[]);
this.tree._set("selectedNodes",_19);
this.tree._set("selectedNode",_19[0]||null);
this.tree._set("selectedItems",_1b);
this.tree._set("selectedItem",_1b[0]||null);
},onClickPress:function(e){
if(this.current&&this.current.isExpandable&&this.tree.isExpandoNode(e.target,this.current)){
return;
}
if(e.type=="mousedown"&&_7.isLeft(e)){
e.preventDefault();
}
var _1d=e.type=="keydown"?this.tree.focusedChild:this.current;
if(!_1d){
return;
}
var _1e=_5.getCopyKeyState(e),id=_1d.id;
if(!this.singular&&!e.shiftKey&&this.selection[id]){
this._doDeselect=true;
return;
}else{
this._doDeselect=false;
}
this.userSelect(_1d,_1e,e.shiftKey);
},onClickRelease:function(e){
if(!this._doDeselect){
return;
}
this._doDeselect=false;
this.userSelect(e.type=="keyup"?this.tree.focusedChild:this.current,_5.getCopyKeyState(e),e.shiftKey);
},onMouseMove:function(){
this._doDeselect=false;
},onMouseDown:function(){
},onMouseUp:function(){
},_compareNodes:function(n1,n2){
if(n1===n2){
return 0;
}
if("sourceIndex" in document.documentElement){
return n1.sourceIndex-n2.sourceIndex;
}else{
if("compareDocumentPosition" in document.documentElement){
return n1.compareDocumentPosition(n2)&2?1:-1;
}else{
if(document.createRange){
var r1=doc.createRange();
r1.setStartBefore(n1);
var r2=doc.createRange();
r2.setStartBefore(n2);
return r1.compareBoundaryPoints(r1.END_TO_END,r2);
}else{
throw Error("dijit.tree._compareNodes don't know how to compare two different nodes in this browser");
}
}
}
},userSelect:function(_1f,_20,_21){
if(this.singular){
if(this.anchor==_1f&&_20){
this.selectNone();
}else{
this.setSelection([_1f]);
this.anchor=_1f;
}
}else{
if(_21&&this.anchor){
var cr=this._compareNodes(this.anchor.rowNode,_1f.rowNode),_22,end,_23=this.anchor;
if(cr<0){
_22=_23;
end=_1f;
}else{
_22=_1f;
end=_23;
}
var _24=[];
while(_22!=end){
_24.push(_22);
_22=this.tree._getNext(_22);
}
_24.push(end);
this.setSelection(_24);
}else{
if(this.selection[_1f.id]&&_20){
this.removeTreeNode(_1f);
}else{
if(_20){
this.addTreeNode(_1f,true);
}else{
this.setSelection([_1f]);
this.anchor=_1f;
}
}
}
}
},getItem:function(key){
var _25=this.selection[key];
return {data:_25,type:["treeNode"]};
},forInSelectedItems:function(f,o){
o=o||_3.global;
for(var id in this.selection){
f.call(o,this.getItem(id),id,this);
}
}});
});
