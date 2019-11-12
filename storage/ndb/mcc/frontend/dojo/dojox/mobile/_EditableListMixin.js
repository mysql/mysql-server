//>>built
define("dojox/mobile/_EditableListMixin",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/touch","dijit/registry","./ListItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _3("dojox.mobile._EditableListMixin",null,{rightIconForEdit:"mblDomButtonGrayKnob",deleteIconForEdit:"mblDomButtonRedCircleMinus",isEditing:false,destroy:function(){
if(this._blankItem){
this._blankItem.destroy();
}
this.inherited(arguments);
},_setupMoveItem:function(_b){
_7.set(_b,{width:_6.getContentBox(_b).w+"px",top:_b.offsetTop+"px"});
_5.add(_b,"mblListItemFloat");
},_resetMoveItem:function(_c){
setTimeout(function(){
_5.remove(_c,"mblListItemFloat");
_7.set(_c,{width:"",top:""});
},0);
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
var _d=_9.getEnclosingWidget(e.target);
for(var n=e.target;n!==_d.domNode;n=n.parentNode){
if(n===_d.deleteIconNode){
_2.publish("/dojox/mobile/deleteListItem",[_d]);
break;
}
}
},onClick:function(){
},_onTouchStart:function(e){
if(this.getChildren().length<=1){
return;
}
if(!this._blankItem){
this._blankItem=new _a();
}
var _e=this._movingItem=_9.getEnclosingWidget(e.target);
var _f=false;
for(var n=e.target;n!==_e.domNode;n=n.parentNode){
if(n===_e.rightIconNode){
_f=true;
break;
}
}
if(!_f){
return;
}
var ref=_e.getNextSibling();
ref=ref?ref.domNode:null;
this.containerNode.insertBefore(this._blankItem.domNode,ref);
this._setupMoveItem(_e.domNode);
this.containerNode.appendChild(_e.domNode);
if(!this._conn){
this._conn=[this.connect(this.domNode,_8.move,"_onTouchMove"),this.connect(this.domNode,_8.release,"_onTouchEnd")];
}
this._pos=[];
_1.forEach(this.getChildren(),function(c,_10){
this._pos.push(_6.position(c.domNode,true).y);
},this);
this.touchStartY=e.touches?e.touches[0].pageY:e.pageY;
this._startTop=_6.getMarginBox(_e.domNode).t;
_4.stop(e);
},_onTouchMove:function(e){
var y=e.touches?e.touches[0].pageY:e.pageY;
var _11=this._pos.length-1;
for(var i=1;i<this._pos.length;i++){
if(y<this._pos[i]){
_11=i-1;
break;
}
}
var _12=this.getChildren()[_11];
var _13=this._blankItem;
if(_12!==_13){
var p=_12.domNode.parentNode;
if(_12.getIndexInParent()<_13.getIndexInParent()){
p.insertBefore(_13.domNode,_12.domNode);
}else{
p.insertBefore(_12.domNode,_13.domNode);
}
}
this._movingItem.domNode.style.top=this._startTop+(y-this.touchStartY)+"px";
},_onTouchEnd:function(e){
var ref=this._blankItem.getNextSibling();
ref=ref?ref.domNode:null;
this.containerNode.insertBefore(this._movingItem.domNode,ref);
this.containerNode.removeChild(this._blankItem.domNode);
this._resetMoveItem(this._movingItem.domNode);
_1.forEach(this._conn,_2.disconnect);
this._conn=null;
},startEdit:function(){
this.isEditing=true;
_5.add(this.domNode,"mblEditableRoundRectList");
_1.forEach(this.getChildren(),function(_14){
if(!_14.deleteIconNode){
_14.set("rightIcon",this.rightIconForEdit);
_14.set("deleteIcon",this.deleteIconForEdit);
_14.deleteIconNode.tabIndex=_14.tabIndex;
}
_14.rightIconNode.style.display="";
_14.deleteIconNode.style.display="";
},this);
if(!this._handles){
this._handles=[this.connect(this.domNode,_8.press,"_onTouchStart"),this.connect(this.domNode,"onclick","_onClick"),this.connect(this.domNode,"onkeydown","_onClick")];
}
},endEdit:function(){
_5.remove(this.domNode,"mblEditableRoundRectList");
_1.forEach(this.getChildren(),function(_15){
_15.rightIconNode.style.display="none";
_15.deleteIconNode.style.display="none";
});
if(this._handles){
_1.forEach(this._handles,this.disconnect,this);
this._handles=null;
}
this.isEditing=false;
}});
});
