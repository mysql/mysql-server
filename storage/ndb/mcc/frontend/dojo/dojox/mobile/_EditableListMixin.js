//>>built
define("dojox/mobile/_EditableListMixin",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/touch","dojo/dom-attr","dijit/registry","./ListItem","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
return _3("dojox.mobile._EditableListMixin",null,{rightIconForEdit:"mblDomButtonGrayKnob",deleteIconForEdit:"mblDomButtonRedCircleMinus",isEditing:false,destroy:function(){
if(this._blankItem){
this._blankItem.destroy();
}
this.inherited(arguments);
},_setupMoveItem:function(_e){
_8.set(_e,{width:_7.getContentBox(_e).w+"px",top:_e.offsetTop+"px"});
_6.add(_e,"mblListItemFloat");
},_resetMoveItem:function(_f){
this.defer(function(){
_6.remove(_f,"mblListItemFloat");
_8.set(_f,{width:"",top:""});
});
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
var _10=_b.getEnclosingWidget(e.target);
for(var n=e.target;n!==_10.domNode;n=n.parentNode){
if(n===_10.deleteIconNode){
_2.publish("/dojox/mobile/deleteListItem",[_10]);
this.onDeleteItem(_10);
break;
}
}
},onClick:function(){
},_onTouchStart:function(e){
if(this.getChildren().length<=1){
return;
}
if(!this._blankItem){
this._blankItem=new _c();
}
var _11=this._movingItem=_b.getEnclosingWidget(e.target);
this._startIndex=this.getIndexOfChild(_11);
var _12=false;
for(var n=e.target;n!==_11.domNode;n=n.parentNode){
if(n===_11.rightIconNode){
_12=true;
_a.set(_11.rightIconNode,"aria-grabbed","true");
_a.set(this.domNode,"aria-dropeffect","move");
break;
}
}
if(!_12){
return;
}
var ref=_11.getNextSibling();
ref=ref?ref.domNode:null;
this.containerNode.insertBefore(this._blankItem.domNode,ref);
this._setupMoveItem(_11.domNode);
this.containerNode.appendChild(_11.domNode);
if(!this._conn){
this._conn=[this.connect(this.domNode,_9.move,"_onTouchMove"),this.connect(_5.doc,_9.release,"_onTouchEnd")];
}
this._pos=[];
_1.forEach(this.getChildren(),function(c,_13){
this._pos.push(_7.position(c.domNode,true).y);
},this);
this.touchStartY=e.touches?e.touches[0].pageY:e.pageY;
this._startTop=_7.getMarginBox(_11.domNode).t;
_4.stop(e);
},_onTouchMove:function(e){
var y=e.touches?e.touches[0].pageY:e.pageY;
var _14=this._pos.length-1;
for(var i=1;i<this._pos.length;i++){
if(y<this._pos[i]){
_14=i-1;
break;
}
}
var _15=this.getChildren()[_14];
var _16=this._blankItem;
if(_15!==_16){
var p=_15.domNode.parentNode;
if(_15.getIndexInParent()<_16.getIndexInParent()){
p.insertBefore(_16.domNode,_15.domNode);
}else{
p.insertBefore(_15.domNode,_16.domNode);
}
}
this._movingItem.domNode.style.top=this._startTop+(y-this.touchStartY)+"px";
},_onTouchEnd:function(e){
var _17=this._startIndex;
var _18=this.getIndexOfChild(this._blankItem);
var ref=this._blankItem.getNextSibling();
ref=ref?ref.domNode:null;
if(ref===null){
_18--;
}
this.containerNode.insertBefore(this._movingItem.domNode,ref);
this.containerNode.removeChild(this._blankItem.domNode);
this._resetMoveItem(this._movingItem.domNode);
_1.forEach(this._conn,_2.disconnect);
this._conn=null;
this.onMoveItem(this._movingItem,_17,_18);
_a.set(this._movingItem.rightIconNode,"aria-grabbed","false");
_a.remove(this.domNode,"aria-dropeffect");
},startEdit:function(){
this.isEditing=true;
_6.add(this.domNode,"mblEditableRoundRectList");
_1.forEach(this.getChildren(),function(_19){
if(!_19.deleteIconNode){
_19.set("rightIcon",this.rightIconForEdit);
if(_19.rightIconNode){
_a.set(_19.rightIconNode,"role","button");
_a.set(_19.rightIconNode,"aria-grabbed","false");
}
_19.set("deleteIcon",this.deleteIconForEdit);
_19.deleteIconNode.tabIndex=_19.tabIndex;
if(_19.deleteIconNode){
_a.set(_19.deleteIconNode,"role","button");
}
}
_19.rightIconNode.style.display="";
_19.deleteIconNode.style.display="";
_d._setTouchAction(_19.rightIconNode,"none");
},this);
if(!this._handles){
this._handles=[this.connect(this.domNode,_9.press,"_onTouchStart"),this.connect(this.domNode,"onclick","_onClick"),this.connect(this.domNode,"onkeydown","_onClick")];
}
this.onStartEdit();
},endEdit:function(){
_6.remove(this.domNode,"mblEditableRoundRectList");
_1.forEach(this.getChildren(),function(_1a){
_1a.rightIconNode.style.display="none";
_1a.deleteIconNode.style.display="none";
_d._setTouchAction(_1a.rightIconNode,"auto");
});
if(this._handles){
_1.forEach(this._handles,this.disconnect,this);
this._handles=null;
}
this.isEditing=false;
this.onEndEdit();
},onDeleteItem:function(_1b){
},onMoveItem:function(_1c,_1d,to){
},onStartEdit:function(){
},onEndEdit:function(){
}});
});
