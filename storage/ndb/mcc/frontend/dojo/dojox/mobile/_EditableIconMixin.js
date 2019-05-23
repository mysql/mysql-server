//>>built
define("dojox/mobile/_EditableIconMixin",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/dom-geometry","dojo/dom-style","dojo/touch","dijit/registry","./IconItem","./sniff","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
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
this._handles=[this.connect(this.domNode,"webkitTransitionStart","_onTransitionStart"),this.connect(this.domNode,"webkitTransitionEnd","_onTransitionEnd")];
}
var _d=0;
_1.forEach(this.getChildren(),function(w){
setTimeout(_5.hitch(this,function(){
w.set("deleteIcon",this.deleteIconForEdit);
if(w.deleteIconNode){
w._deleteHandle=this.connect(w.deleteIconNode,"onclick","_deleteIconClicked");
}
w.highlight(0);
}),15*_d++);
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
},scaleItem:function(_e,_f){
_7.set(_e.domNode,{webkitTransition:_b("android")?"":"-webkit-transform .1s ease-in-out",webkitTransform:_f==1?"":"scale("+_f+")"});
},_onTransitionStart:function(e){
_4.stop(e);
},_onTransitionEnd:function(e){
_4.stop(e);
var w=_9.getEnclosingWidget(e.target);
w._moving=false;
_7.set(w.domNode,"webkitTransition","");
},_onTouchStart:function(e){
if(!this._blankItem){
this._blankItem=new _a();
this._blankItem.domNode.style.visibility="hidden";
this._blankItem._onClick=function(){
};
}
var _10=this._movingItem=_9.getEnclosingWidget(e.target);
var _11=false;
for(var n=e.target;n!==_10.domNode;n=n.parentNode){
if(n===_10.iconNode){
_11=true;
break;
}
}
if(!_11){
return;
}
if(!this._conn){
this._conn=[this.connect(this.domNode,_b("touch")?"ontouchmove":"onmousemove","_onTouchMove"),this.connect(this.domNode,_b("touch")?"ontouchend":"onmouseup","_onTouchEnd")];
}
this._touchStartPosX=e.touches?e.touches[0].pageX:e.pageX;
this._touchStartPosY=e.touches?e.touches[0].pageY:e.pageY;
if(this.isEditing){
this._onDragStart(e);
}else{
this._pressTimer=setTimeout(_5.hitch(this,function(){
this.startEdit();
this._onDragStart(e);
}),1000);
}
},_onDragStart:function(e){
this._dragging=true;
var _12=this._movingItem;
if(_12.get("selected")){
_12.set("selected",false);
}
this.scaleItem(_12,1.1);
var x=e.touches?e.touches[0].pageX:e.pageX;
var y=e.touches?e.touches[0].pageY:e.pageY;
var _13=_c.getEnclosingScrollable(_12.domNode);
var dx=0;
var dy=0;
if(_13){
var pos=_13.getPos();
dx=pos.x;
dy=pos.y;
_4.stop(e);
}
var _14=this._startPos=_6.position(_12.domNode,true);
this._offsetPos={x:_14.x-x-dx,y:_14.y-y-dy};
this._startIndex=this.getIndexOfChild(_12);
this.addChild(this._blankItem,this._startIndex);
this.moveChild(_12,this.getChildren().length);
_7.set(_12.domNode,{position:"absolute",top:(_14.y-dy)+"px",left:(_14.x-dx)+"px",zIndex:100});
},_onTouchMove:function(e){
var x=e.touches?e.touches[0].pageX:e.pageX;
var y=e.touches?e.touches[0].pageY:e.pageY;
if(this._dragging){
_7.set(this._movingItem.domNode,{top:(this._offsetPos.y+y)+"px",left:(this._offsetPos.x+x)+"px"});
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
var _15=this._movingItem;
this.scaleItem(_15,1);
_7.set(_15.domNode,{position:"",top:"",left:"",zIndex:""});
var _16=this._startIndex;
var _17=this.getIndexOfChild(this._blankItem);
this.moveChild(_15,_17);
this.removeChild(this._blankItem);
_2.publish("/dojox/mobile/moveIconItem",[this,_15,_16,_17]);
this.onMoveItem(_15,_16,_17);
}
},_clearPressTimer:function(){
if(this._pressTimer){
clearTimeout(this._pressTimer);
this._pressTimer=null;
}
},_detectOverlap:function(_18){
var _19=this.getChildren(),_1a=this._blankItem,_1b=_6.position(_1a.domNode,true),_1c=this.getIndexOfChild(_1a),dir=1;
if(this._contains(_18,_1b)){
return;
}else{
if(_18.y<_1b.y||(_18.y<=_1b.y+_1b.h&&_18.x<_1b.x)){
dir=-1;
}
}
for(var i=_1c+dir;i>=0&&i<_19.length-1;i+=dir){
var w=_19[i];
if(w._moving){
continue;
}
var pos=_6.position(w.domNode,true);
if(this._contains(_18,pos)){
setTimeout(_5.hitch(this,function(){
this.moveChildWithAnimation(_1a,dir==1?i+1:i);
}),0);
break;
}else{
if((dir==1&&pos.y>_18.y)||(dir==-1&&pos.y+pos.h<_18.y)){
break;
}
}
}
},_contains:function(_1d,pos){
return pos.x<_1d.x&&_1d.x<pos.x+pos.w&&pos.y<_1d.y&&_1d.y<pos.y+pos.h;
},_animate:function(_1e,to){
if(_1e==to){
return;
}
var dir=_1e<to?1:-1;
var _1f=this.getChildren();
var _20=[];
var i;
for(i=_1e;i!=to;i+=dir){
_20.push({t:(_1f[i+dir].domNode.offsetTop-_1f[i].domNode.offsetTop)+"px",l:(_1f[i+dir].domNode.offsetLeft-_1f[i].domNode.offsetLeft)+"px"});
}
for(i=_1e,j=0;i!=to;i+=dir,j++){
var w=_1f[i];
w._moving=true;
_7.set(w.domNode,{top:_20[j].t,left:_20[j].l});
setTimeout(_5.hitch(w,function(){
_7.set(this.domNode,{webkitTransition:"top .3s ease-in-out, left .3s ease-in-out",top:"0px",left:"0px"});
}),j*10);
}
},removeChildWithAnimation:function(_21){
var _22=(typeof _21==="number")?_21:this.getIndexOfChild(_21);
this.removeChild(_21);
this.addChild(this._blankItem);
this._animate(_22,this.getChildren().length-1);
this.removeChild(this._blankItem);
},moveChild:function(_23,_24){
this.addChild(_23,_24);
this.paneContainerWidget.addChild(_23.paneWidget,_24);
},moveChildWithAnimation:function(_25,_26){
var _27=this.getIndexOfChild(this._blankItem);
this.moveChild(_25,_26);
this._animate(_27,_26);
},_deleteIconClicked:function(e){
if(this.deleteIconClicked(e)===false){
return;
}
var _28=_9.getEnclosingWidget(e.target);
this.deleteItem(_28);
},deleteIconClicked:function(){
},deleteItem:function(_29){
if(_29._deleteHandle){
this.disconnect(_29._deleteHandle);
}
this.removeChildWithAnimation(_29);
_2.publish("/dojox/mobile/deleteIconItem",[this,_29]);
this.onDeleteItem(_29);
_29.destroy();
},onDeleteItem:function(_2a){
},onMoveItem:function(_2b,_2c,to){
},onStartEdit:function(){
},onEndEdit:function(){
},_setEditableAttr:function(_2d){
this._set("editable",_2d);
if(_2d&&!this._touchStartHandle){
this._touchStartHandle=this.connect(this.domNode,_8.press,"_onTouchStart");
}else{
if(!_2d&&this._touchStartHandle){
this.disconnect(this._touchStartHandle);
this._touchStartHandle=null;
}
}
}});
});
