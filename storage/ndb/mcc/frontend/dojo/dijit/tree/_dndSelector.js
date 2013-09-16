//>>built
define("dijit/tree/_dndSelector",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/mouse","dojo/on","dojo/touch","dojo/_base/window","./_dndContainer"],function(_1,_2,_3,_4,_5,on,_6,_7,_8){
return _3("dijit.tree._dndSelector",_8,{constructor:function(){
this.selection={};
this.anchor=null;
this.tree.domNode.setAttribute("aria-multiselect",!this.singular);
this.events.push(on(this.tree.domNode,_6.press,_4.hitch(this,"onMouseDown")),on(this.tree.domNode,_6.release,_4.hitch(this,"onMouseUp")),on(this.tree.domNode,_6.move,_4.hitch(this,"onMouseMove")));
},singular:false,getSelectedTreeNodes:function(){
var _9=[],_a=this.selection;
for(var i in _a){
_9.push(_a[i]);
}
return _9;
},selectNone:function(){
this.setSelection([]);
return this;
},destroy:function(){
this.inherited(arguments);
this.selection=this.anchor=null;
},addTreeNode:function(_b,_c){
this.setSelection(this.getSelectedTreeNodes().concat([_b]));
if(_c){
this.anchor=_b;
}
return _b;
},removeTreeNode:function(_d){
this.setSelection(this._setDifference(this.getSelectedTreeNodes(),[_d]));
return _d;
},isTreeNodeSelected:function(_e){
return _e.id&&!!this.selection[_e.id];
},setSelection:function(_f){
var _10=this.getSelectedTreeNodes();
_1.forEach(this._setDifference(_10,_f),_4.hitch(this,function(_11){
_11.setSelected(false);
if(this.anchor==_11){
delete this.anchor;
}
delete this.selection[_11.id];
}));
_1.forEach(this._setDifference(_f,_10),_4.hitch(this,function(_12){
_12.setSelected(true);
this.selection[_12.id]=_12;
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
var _13=this.getSelectedTreeNodes();
var _14=[],_15=[];
_1.forEach(_13,function(_16){
_15.push(_16);
_14.push(_16.getTreePath());
});
var _17=_1.map(_15,function(_18){
return _18.item;
});
this.tree._set("paths",_14);
this.tree._set("path",_14[0]||[]);
this.tree._set("selectedNodes",_15);
this.tree._set("selectedNode",_15[0]||null);
this.tree._set("selectedItems",_17);
this.tree._set("selectedItem",_17[0]||null);
},onMouseDown:function(e){
if(!this.current||this.tree.isExpandoNode(e.target,this.current)){
return;
}
if(!_5.isLeft(e)){
return;
}
e.preventDefault();
var _19=this.current,_1a=_2.isCopyKey(e),id=_19.id;
if(!this.singular&&!e.shiftKey&&this.selection[id]){
this._doDeselect=true;
return;
}else{
this._doDeselect=false;
}
this.userSelect(_19,_1a,e.shiftKey);
},onMouseUp:function(e){
if(!this._doDeselect){
return;
}
this._doDeselect=false;
this.userSelect(this.current,_2.isCopyKey(e),e.shiftKey);
},onMouseMove:function(){
this._doDeselect=false;
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
},userSelect:function(_1b,_1c,_1d){
if(this.singular){
if(this.anchor==_1b&&_1c){
this.selectNone();
}else{
this.setSelection([_1b]);
this.anchor=_1b;
}
}else{
if(_1d&&this.anchor){
var cr=this._compareNodes(this.anchor.rowNode,_1b.rowNode),_1e,end,_1f=this.anchor;
if(cr<0){
_1e=_1f;
end=_1b;
}else{
_1e=_1b;
end=_1f;
}
var _20=[];
while(_1e!=end){
_20.push(_1e);
_1e=this.tree._getNextNode(_1e);
}
_20.push(end);
this.setSelection(_20);
}else{
if(this.selection[_1b.id]&&_1c){
this.removeTreeNode(_1b);
}else{
if(_1c){
this.addTreeNode(_1b,true);
}else{
this.setSelection([_1b]);
this.anchor=_1b;
}
}
}
}
},getItem:function(key){
var _21=this.selection[key];
return {data:_21,type:["treeNode"]};
},forInSelectedItems:function(f,o){
o=o||_7.global;
for(var id in this.selection){
f.call(o,this.getItem(id),id,this);
}
}});
});
