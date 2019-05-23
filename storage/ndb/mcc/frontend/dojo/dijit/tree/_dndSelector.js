//>>built
define("dijit/tree/_dndSelector",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/Deferred","dojo/_base/kernel","dojo/_base/lang","dojo/cookie","dojo/mouse","dojo/on","dojo/touch","./_dndContainer"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a){
return _3("dijit.tree._dndSelector",_a,{constructor:function(){
this.selection={};
this.anchor=null;
if(!this.cookieName&&this.tree.id){
this.cookieName=this.tree.id+"SaveSelectedCookie";
}
this.events.push(on(this.tree.domNode,_9.press,_6.hitch(this,"onMouseDown")),on(this.tree.domNode,_9.release,_6.hitch(this,"onMouseUp")),on(this.tree.domNode,_9.move,_6.hitch(this,"onMouseMove")));
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
this.setSelection(this._setDifference(this.getSelectedTreeNodes(),[_f]));
return _f;
},isTreeNodeSelected:function(_10){
return _10.id&&!!this.selection[_10.id];
},setSelection:function(_11){
var _12=this.getSelectedTreeNodes();
_1.forEach(this._setDifference(_12,_11),_6.hitch(this,function(_13){
_13.setSelected(false);
if(this.anchor==_13){
delete this.anchor;
}
delete this.selection[_13.id];
}));
_1.forEach(this._setDifference(_11,_12),_6.hitch(this,function(_14){
_14.setSelected(true);
this.selection[_14.id]=_14;
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
var _15=this.getSelectedTreeNodes();
var _16=[],_17=[],_18=[];
_1.forEach(_15,function(_19){
var ary=_19.getTreePath(),_1a=this.tree.model;
_17.push(_19);
_16.push(ary);
ary=_1.map(ary,function(_1b){
return _1a.getIdentity(_1b);
},this);
_18.push(ary.join("/"));
},this);
var _1c=_1.map(_17,function(_1d){
return _1d.item;
});
this.tree._set("paths",_16);
this.tree._set("path",_16[0]||[]);
this.tree._set("selectedNodes",_17);
this.tree._set("selectedNode",_17[0]||null);
this.tree._set("selectedItems",_1c);
this.tree._set("selectedItem",_1c[0]||null);
if(this.tree.persist&&_18.length>0){
_7(this.cookieName,_18.join(","),{expires:365});
}
},_getSavedPaths:function(){
var _1e=this.tree;
if(_1e.persist&&_1e.dndController.cookieName){
var _1f,_20=[];
_1f=_7(_1e.dndController.cookieName);
if(_1f){
_20=_1.map(_1f.split(","),function(_21){
return _21.split("/");
});
}
return _20;
}
},onMouseDown:function(e){
if(!this.current||this.tree.isExpandoNode(e.target,this.current)){
return;
}
if(e.type=="mousedown"&&_8.isLeft(e)){
e.preventDefault();
}else{
if(e.type!="touchstart"){
return;
}
}
var _22=this.current,_23=_2.isCopyKey(e),id=_22.id;
if(!this.singular&&!e.shiftKey&&this.selection[id]){
this._doDeselect=true;
return;
}else{
this._doDeselect=false;
}
this.userSelect(_22,_23,e.shiftKey);
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
},userSelect:function(_24,_25,_26){
if(this.singular){
if(this.anchor==_24&&_25){
this.selectNone();
}else{
this.setSelection([_24]);
this.anchor=_24;
}
}else{
if(_26&&this.anchor){
var cr=this._compareNodes(this.anchor.rowNode,_24.rowNode),_27,end,_28=this.anchor;
if(cr<0){
_27=_28;
end=_24;
}else{
_27=_24;
end=_28;
}
var _29=[];
while(_27!=end){
_29.push(_27);
_27=this.tree._getNextNode(_27);
}
_29.push(end);
this.setSelection(_29);
}else{
if(this.selection[_24.id]&&_25){
this.removeTreeNode(_24);
}else{
if(_25){
this.addTreeNode(_24,true);
}else{
this.setSelection([_24]);
this.anchor=_24;
}
}
}
}
},getItem:function(key){
var _2a=this.selection[key];
return {data:_2a,type:["treeNode"]};
},forInSelectedItems:function(f,o){
o=o||_5.global;
for(var id in this.selection){
f.call(o,this.getItem(id),id,this);
}
}});
});
