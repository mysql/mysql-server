//>>built
define("dijit/tree/dndSource",["dojo/_base/array","dojo/_base/declare","dojo/dnd/common","dojo/dom-class","dojo/dom-geometry","dojo/_base/lang","dojo/mouse","dojo/on","dojo/touch","dojo/topic","dojo/dnd/Manager","./_dndSelector"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b){
var _c=_2("dijit.tree.dndSource",_b,{isSource:true,accept:["text","treeNode"],copyOnly:false,dragThreshold:5,betweenThreshold:0,generateText:true,constructor:function(_d,_e){
if(!_e){
_e={};
}
_6.mixin(this,_e);
var _f=_e.accept instanceof Array?_e.accept:["text","treeNode"];
this.accept=null;
if(_f.length){
this.accept={};
for(var i=0;i<_f.length;++i){
this.accept[_f[i]]=1;
}
}
this.isDragging=false;
this.mouseDown=false;
this.targetAnchor=null;
this.targetBox=null;
this.dropPosition="";
this._lastX=0;
this._lastY=0;
this.sourceState="";
if(this.isSource){
_4.add(this.node,"dojoDndSource");
}
this.targetState="";
if(this.accept){
_4.add(this.node,"dojoDndTarget");
}
this.topics=[_9.subscribe("/dnd/source/over",_6.hitch(this,"onDndSourceOver")),_9.subscribe("/dnd/start",_6.hitch(this,"onDndStart")),_9.subscribe("/dnd/drop",_6.hitch(this,"onDndDrop")),_9.subscribe("/dnd/cancel",_6.hitch(this,"onDndCancel"))];
},checkAcceptance:function(){
return true;
},copyState:function(_10){
return this.copyOnly||_10;
},destroy:function(){
this.inherited(arguments);
var h;
while(h=this.topics.pop()){
h.remove();
}
this.targetAnchor=null;
},_onDragMouse:function(e,_11){
var m=_a.manager(),_12=this.targetAnchor,_13=this.current,_14=this.dropPosition;
var _15="Over";
if(_13&&this.betweenThreshold>0){
if(!this.targetBox||_12!=_13){
this.targetBox=_5.position(_13.rowNode,true);
}
if((e.pageY-this.targetBox.y)<=this.betweenThreshold){
_15="Before";
}else{
if((e.pageY-this.targetBox.y)>=(this.targetBox.h-this.betweenThreshold)){
_15="After";
}
}
}
if(_11||_13!=_12||_15!=_14){
if(_12){
this._removeItemClass(_12.rowNode,_14);
}
if(_13){
this._addItemClass(_13.rowNode,_15);
}
if(!_13){
m.canDrop(false);
}else{
if(_13==this.tree.rootNode&&_15!="Over"){
m.canDrop(false);
}else{
var _16=false,_17=false;
if(m.source==this){
_17=(_15==="Over");
for(var _18 in this.selection){
var _19=this.selection[_18];
if(_19.item===_13.item){
_16=true;
break;
}
if(_19.getParent().id!==_13.id){
_17=false;
}
}
}
m.canDrop(!_16&&!_17&&!this._isParentChildDrop(m.source,_13.rowNode)&&this.checkItemAcceptance(_13.rowNode,m.source,_15.toLowerCase()));
}
}
this.targetAnchor=_13;
this.dropPosition=_15;
}
},onMouseMove:function(e){
if(this.isDragging&&this.targetState=="Disabled"){
return;
}
this.inherited(arguments);
var m=_a.manager();
if(this.isDragging){
this._onDragMouse(e);
}else{
if(this.mouseDown&&this.isSource&&(Math.abs(e.pageX-this._lastX)>=this.dragThreshold||Math.abs(e.pageY-this._lastY)>=this.dragThreshold)){
var _1a=this.getSelectedTreeNodes();
if(_1a.length){
if(_1a.length>1){
var _1b=this.selection,i=0,r=[],n,p;
nextitem:
while((n=_1a[i++])){
for(p=n.getParent();p&&p!==this.tree;p=p.getParent()){
if(_1b[p.id]){
continue nextitem;
}
}
r.push(n);
}
_1a=r;
}
_1a=_1.map(_1a,function(n){
return n.domNode;
});
m.startDrag(this,_1a,this.copyState(_3.getCopyKeyState(e)));
this._onDragMouse(e,true);
}
}
}
},onMouseDown:function(e){
if(e.type=="touchstart"||_7.isLeft(e)){
this.mouseDown=true;
this.mouseButton=e.button;
this._lastX=e.pageX;
this._lastY=e.pageY;
}
this.inherited(arguments);
},onMouseUp:function(e){
if(this.mouseDown){
this.mouseDown=false;
this.inherited(arguments);
}
},onMouseOut:function(){
this.inherited(arguments);
this._unmarkTargetAnchor();
},checkItemAcceptance:function(){
return true;
},onDndSourceOver:function(_1c){
if(this!=_1c){
this.mouseDown=false;
this._unmarkTargetAnchor();
}else{
if(this.isDragging){
var m=_a.manager();
m.canDrop(false);
}
}
},onDndStart:function(_1d,_1e,_1f){
if(this.isSource){
this._changeState("Source",this==_1d?(_1f?"Copied":"Moved"):"");
}
var _20=this.checkAcceptance(_1d,_1e);
this._changeState("Target",_20?"":"Disabled");
if(this==_1d){
_a.manager().overSource(this);
}
this.isDragging=true;
},itemCreator:function(_21){
return _1.map(_21,function(_22){
return {"id":_22.id,"name":_22.textContent||_22.innerText||""};
});
},onDndDrop:function(_23,_24,_25){
if(this.containerState=="Over"){
var _26=this.tree,_27=_26.model,_28=this.targetAnchor,_29=false;
this.isDragging=false;
var _2a;
var _2b;
var _2c;
_2a=(_28&&_28.item)||_26.item;
if(this.dropPosition=="Before"||this.dropPosition=="After"){
_2a=(_28.getParent()&&_28.getParent().item)||_26.item;
_2b=_28.getIndexInParent();
if(this.dropPosition=="After"){
_2b=_28.getIndexInParent()+1;
_2c=_28.getNextSibling()&&_28.getNextSibling().item;
}else{
_2c=_28.item;
}
}else{
_2a=(_28&&_28.item)||_26.item;
_29=true;
}
var _2d;
_1.forEach(_24,function(_2e,idx){
var _2f=_23.getItem(_2e.id);
if(_1.indexOf(_2f.type,"treeNode")!=-1){
var _30=_2f.data,_31=_30.item,_32=_30.getParent().item;
}
if(_23==this){
if(typeof _2b=="number"){
if(_2a==_32&&_30.getIndexInParent()<_2b){
_2b-=1;
}
}
_27.pasteItem(_31,_32,_2a,_25,_2b,_2c);
}else{
if(_27.isItem(_31)){
_27.pasteItem(_31,_32,_2a,_25,_2b,_2c);
}else{
if(!_2d){
_2d=this.itemCreator(_24,_28.rowNode,_23);
}
_27.newItem(_2d[idx],_2a,_2b,_2c);
}
}
},this);
if(_29){
this.tree._expandNode(_28);
}
}
this.onDndCancel();
},onDndCancel:function(){
this._unmarkTargetAnchor();
this.isDragging=false;
this.mouseDown=false;
delete this.mouseButton;
this._changeState("Source","");
this._changeState("Target","");
},onOverEvent:function(){
this.inherited(arguments);
_a.manager().overSource(this);
},onOutEvent:function(){
this._unmarkTargetAnchor();
var m=_a.manager();
if(this.isDragging){
m.canDrop(false);
}
m.outSource(this);
this.inherited(arguments);
},_isParentChildDrop:function(_33,_34){
if(!_33.tree||_33.tree!=this.tree){
return false;
}
var _35=_33.tree.domNode;
var ids=_33.selection;
var _36=_34.parentNode;
while(_36!=_35&&!ids[_36.id]){
_36=_36.parentNode;
}
return _36.id&&ids[_36.id];
},_unmarkTargetAnchor:function(){
if(!this.targetAnchor){
return;
}
this._removeItemClass(this.targetAnchor.rowNode,this.dropPosition);
this.targetAnchor=null;
this.targetBox=null;
this.dropPosition=null;
},_markDndStatus:function(_37){
this._changeState("Source",_37?"Copied":"Moved");
}});
return _c;
});
