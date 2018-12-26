//>>built
define("dijit/form/_ListBase",["dojo/_base/declare","dojo/window"],function(_1,_2){
return _1("dijit.form._ListBase",null,{selected:null,_getTarget:function(_3){
var _4=_3.target;
var _5=this.containerNode;
if(_4==_5||_4==this.domNode){
return null;
}
while(_4&&_4.parentNode!=_5){
_4=_4.parentNode;
}
return _4;
},selectFirstNode:function(){
var _6=this.containerNode.firstChild;
while(_6&&_6.style.display=="none"){
_6=_6.nextSibling;
}
this._setSelectedAttr(_6);
},selectLastNode:function(){
var _7=this.containerNode.lastChild;
while(_7&&_7.style.display=="none"){
_7=_7.previousSibling;
}
this._setSelectedAttr(_7);
},selectNextNode:function(){
var _8=this._getSelectedAttr();
if(!_8){
this.selectFirstNode();
}else{
var _9=_8.nextSibling;
while(_9&&_9.style.display=="none"){
_9=_9.nextSibling;
}
if(!_9){
this.selectFirstNode();
}else{
this._setSelectedAttr(_9);
}
}
},selectPreviousNode:function(){
var _a=this._getSelectedAttr();
if(!_a){
this.selectLastNode();
}else{
var _b=_a.previousSibling;
while(_b&&_b.style.display=="none"){
_b=_b.previousSibling;
}
if(!_b){
this.selectLastNode();
}else{
this._setSelectedAttr(_b);
}
}
},_setSelectedAttr:function(_c){
if(this.selected!=_c){
var _d=this._getSelectedAttr();
if(_d){
this.onDeselect(_d);
this.selected=null;
}
if(_c&&_c.parentNode==this.containerNode){
this.selected=_c;
_2.scrollIntoView(_c);
this.onSelect(_c);
}
}else{
if(_c){
this.onSelect(_c);
}
}
},_getSelectedAttr:function(){
var v=this.selected;
return (v&&v.parentNode==this.containerNode)?v:(this.selected=null);
}});
});
