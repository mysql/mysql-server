//>>built
define("dojox/mobile/_EditableIconMixin",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojo/dom-style","dojo/touch","dijit/registry","./IconItem","./sniff","./viewRegistry","./_css3"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
return _3("dojox.mobile._EditableIconMixin",null,{deleteIconForEdit:"mblDomButtonBlackCircleCross",threshold:4,destroy:function(){
if(this._blankItem){
this._blankItem.destroy();
}
this.inherited(arguments);
},startEdit:function(){
if(!this.editable||this.isEditing){
return;
}
this.isEditing=true;
if(!this._handles){
this._handles=[this.connect(this.domNode,_e.name("transitionStart"),"_onTransitionStart"),this.connect(this.domNode,_e.name("transitionEnd"),"_onTransitionEnd")];
}
var _f=0;
_1.forEach(this.getChildren(),function(w){
this.defer(function(){
w.set("deleteIcon",this.deleteIconForEdit);
if(w.deleteIconNode){
w._deleteHandle=this.connect(w.deleteIconNode,"onclick","_deleteIconClicked");
}
w.highlight(0);
},15*_f++);
},this);
_2.publish("/dojox/mobile/startEdit",[this]);
this.onStartEdit();
},endEdit:function(){
if(!this.isEditing){
return;
}
_1.forEach(this.getChildren(),function(w){
w.unhighlight();
if(w._deleteHandle){
this.disconnect(w._deleteHandle);
w._deleteHandle=null;
}
w.set("deleteIcon","");
},this);
this._movingItem=null;
if(this._handles){
_1.forEach(this._handles,this.disconnect,this);
this._handles=null;
}
_2.publish("/dojox/mobile/endEdit",[this]);
this.onEndEdit();
this.isEditing=false;
},scaleItem:function(_10,_11){
_8.set(_10.domNode,_e.add({},{transition:_c("android")?"":_e.name("transform",true)+" .1s ease-in-out",transform:_11==1?"":"scale("+_11+")"}));
},_onTransitionStart:function(e){
_4.stop(e);
},_onTransitionEnd:function(e){
_4.stop(e);
var w=_a.getEnclosingWidget(e.target);
w._moving=false;
_8.set(w.domNode,_e.name("transition"),"");
},_onTouchStart:function(e){
if(!this._blankItem){
this._blankItem=new _b();
this._blankItem.domNode.style.visibility="hidden";
this._blankItem._onClick=function(){
};
}
var _12=this._movingItem=_a.getEnclosingWidget(e.target);
var _13=false;
var n;
for(n=e.target;n!==_12.domNode;n=n.parentNode){
if(n===_12.iconNode){
_13=true;
break;
}
}
if(!_13){
return;
}
if(!this._conn){
this._conn=[this.connect(this.domNode,_9.move,"_onTouchMove"),this.connect(_6.doc,_9.release,"_onTouchEnd")];
}
this._touchStartPosX=e.touches?e.touches[0].pageX:e.pageX;
this._touchStartPosY=e.touches?e.touches[0].pageY:e.pageY;
if(this.isEditing){
this._onDragStart(e);
}else{
this._pressTimer=this.defer(function(){
this.startEdit();
this._onDragStart(e);
},1000);
}
},_onDragStart:function(e){
this._dragging=true;
var _14=this._movingItem;
if(_14.get("selected")){
_14.set("selected",false);
}
this.scaleItem(_14,1.1);
var x=e.touches?e.touches[0].pageX:e.pageX;
var y=e.touches?e.touches[0].pageY:e.pageY;
var _15=_d.getEnclosingScrollable(_14.domNode);
var dx=0;
var dy=0;
if(_15){
var pos=_15.getPos();
dx=pos.x;
dy=pos.y;
_4.stop(e);
}
var _16=this._startPos=_7.position(_14.domNode,true);
this._offsetPos={x:_16.x-x-dx,y:_16.y-y-dy};
this._startIndex=this.getIndexOfChild(_14);
this.addChild(this._blankItem,this._startIndex);
this.moveChild(_14,this.getChildren().length);
_8.set(_14.domNode,{position:"absolute",top:(_16.y-dy)+"px",left:(_16.x-dx)+"px",zIndex:100});
},_onTouchMove:function(e){
var x=e.touches?e.touches[0].pageX:e.pageX;
var y=e.touches?e.touches[0].pageY:e.pageY;
if(this._dragging){
_8.set(this._movingItem.domNode,{top:(this._offsetPos.y+y)+"px",left:(this._offsetPos.x+x)+"px"});
this._detectOverlap({x:x,y:y});
_4.stop(e);
}else{
var dx=Math.abs(this._touchStartPosX-x);
var dy=Math.abs(this._touchStartPosY-y);
if(dx>this.threshold||dy>this.threshold){
this._clearPressTimer();
}
}
},_onTouchEnd:function(e){
this._clearPressTimer();
if(this._conn){
_1.forEach(this._conn,this.disconnect,this);
this._conn=null;
}
if(this._dragging){
this._dragging=false;
var _17=this._movingItem;
this.scaleItem(_17,1);
_8.set(_17.domNode,{position:"",top:"",left:"",zIndex:""});
var _18=this._startIndex;
var _19=this.getIndexOfChild(this._blankItem);
this.moveChild(_17,_19);
this.removeChild(this._blankItem);
_2.publish("/dojox/mobile/moveIconItem",[this,_17,_18,_19]);
this.onMoveItem(_17,_18,_19);
}
},_clearPressTimer:function(){
if(this._pressTimer){
this._pressTimer.remove();
this._pressTimer=null;
}
},_detectOverlap:function(_1a){
var _1b=this.getChildren(),_1c=this._blankItem,_1d=_7.position(_1c.domNode,true),_1e=this.getIndexOfChild(_1c),dir=1,i,w,pos;
if(this._contains(_1a,_1d)){
return;
}else{
if(_1a.y<_1d.y||(_1a.y<=_1d.y+_1d.h&&_1a.x<_1d.x)){
dir=-1;
}
}
for(i=_1e+dir;i>=0&&i<_1b.length-1;i+=dir){
w=_1b[i];
if(w._moving){
continue;
}
pos=_7.position(w.domNode,true);
if(this._contains(_1a,pos)){
this.defer(function(){
this.moveChildWithAnimation(_1c,dir==1?i+1:i);
});
break;
}else{
if((dir==1&&pos.y>_1a.y)||(dir==-1&&pos.y+pos.h<_1a.y)){
break;
}
}
}
},_contains:function(_1f,pos){
return pos.x<_1f.x&&_1f.x<pos.x+pos.w&&pos.y<_1f.y&&_1f.y<pos.y+pos.h;
},_animate:function(_20,to){
if(_20==to){
return;
}
var dir=_20<to?1:-1;
var _21=this.getChildren();
var _22=[];
var i,j;
for(i=_20;i!=to;i+=dir){
_22.push({t:(_21[i+dir].domNode.offsetTop-_21[i].domNode.offsetTop)+"px",l:(_21[i+dir].domNode.offsetLeft-_21[i].domNode.offsetLeft)+"px"});
}
for(i=_20,j=0;i!=to;i+=dir,j++){
var w=_21[i];
w._moving=true;
_8.set(w.domNode,{top:_22[j].t,left:_22[j].l});
this.defer(_5.hitch(w,function(){
_8.set(this.domNode,_e.add({top:"0px",left:"0px"},{transition:"top .3s ease-in-out, left .3s ease-in-out"}));
}),j*10);
}
},removeChildWithAnimation:function(_23){
var _24=(typeof _23==="number")?_23:this.getIndexOfChild(_23);
this.removeChild(_23);
if(this._blankItem){
this.addChild(this._blankItem);
}
this._animate(_24,this.getChildren().length-1);
if(this._blankItem){
this.removeChild(this._blankItem);
}
},moveChild:function(_25,_26){
this.addChild(_25,_26);
this.paneContainerWidget.addChild(_25.paneWidget,_26);
},moveChildWithAnimation:function(_27,_28){
var _29=this.getIndexOfChild(this._blankItem);
this.moveChild(_27,_28);
this._animate(_29,_28);
},_deleteIconClicked:function(e){
if(this.deleteIconClicked(e)===false){
return;
}
var _2a=_a.getEnclosingWidget(e.target);
this.deleteItem(_2a);
},deleteIconClicked:function(){
},deleteItem:function(_2b){
if(_2b._deleteHandle){
this.disconnect(_2b._deleteHandle);
}
this.removeChildWithAnimation(_2b);
_2.publish("/dojox/mobile/deleteIconItem",[this,_2b]);
this.onDeleteItem(_2b);
_2b.destroy();
},onDeleteItem:function(_2c){
},onMoveItem:function(_2d,_2e,to){
},onStartEdit:function(){
},onEndEdit:function(){
},_setEditableAttr:function(_2f){
this._set("editable",_2f);
if(_2f&&!this._touchStartHandle){
this._touchStartHandle=this.connect(this.domNode,_9.press,"_onTouchStart");
}else{
if(!_2f&&this._touchStartHandle){
this.disconnect(this._touchStartHandle);
this._touchStartHandle=null;
}
}
}});
});
