//>>built
define("dojox/mobile/IconItem",["dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","./_ItemBase","./Badge","./TransitionEvent","./iconUtils","./lazyLoadUtils","./viewRegistry","./_css3","dojo/has!dojo-bidi?dojox/mobile/bidi/IconItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11){
var _12=_1(_4("dojo-bidi")?"dojox.mobile.NonBidiIconItem":"dojox.mobile.IconItem",_a,{lazy:false,requires:"",timeout:10,content:"",badge:"",badgeClass:"mblDomButtonRedBadge",deletable:true,deleteIcon:"",tag:"li",paramsToInherit:"transition,icon,deleteIcon,badgeClass,deleteIconTitle,deleteIconRole",baseClass:"mblIconItem",_selStartMethod:"touch",_selEndMethod:"none",destroy:function(){
if(this.badgeObj){
delete this.badgeObj;
}
this.inherited(arguments);
},buildRendering:function(){
this.domNode=this.srcNodeRef||_7.create(this.tag);
if(this.srcNodeRef){
this._tmpNode=_7.create("div");
for(var i=0,len=this.srcNodeRef.childNodes.length;i<len;i++){
this._tmpNode.appendChild(this.srcNodeRef.firstChild);
}
}
this.iconDivNode=_7.create("div",{className:"mblIconArea"},this.domNode);
this.iconParentNode=_7.create("div",{className:"mblIconAreaInner"},this.iconDivNode);
this.labelNode=_7.create("span",{className:"mblIconAreaTitle"},this.iconDivNode);
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
var p=this.getParent();
require([p.iconItemPaneClass],_3.hitch(this,function(_13){
var w=this.paneWidget=new _13(p.iconItemPaneProps);
this.containerNode=w.containerNode;
if(this._tmpNode){
for(var i=0,len=this._tmpNode.childNodes.length;i<len;i++){
w.containerNode.appendChild(this._tmpNode.firstChild);
}
this._tmpNode=null;
}
p.paneContainerWidget.addChild(w,this.getIndexInParent());
w.set("label",this.label);
this._clickCloseHandle=this.connect(w.closeIconNode,"onclick","_closeIconClicked");
this._keydownCloseHandle=this.connect(w.closeIconNode,"onkeydown","_closeIconClicked");
}));
this.inherited(arguments);
if(!this._isOnLine){
this._isOnLine=true;
this.set("icon",this._pendingIcon!==undefined?this._pendingIcon:this.icon);
delete this._pendingIcon;
}
if(!this.icon&&p.defaultIcon){
this.set("icon",p.defaultIcon);
}
this._dragstartHandle=this.connect(this.domNode,"ondragstart",_2.stop);
this.connect(this.domNode,"onkeydown","_onClick");
},highlight:function(_14){
_6.add(this.iconDivNode,"mblVibrate");
_14=(_14!==undefined)?_14:this.timeout;
if(_14>0){
var _15=this;
_15.defer(function(){
_15.unhighlight();
},_14*1000);
}
},unhighlight:function(){
if(!_4("ie")&&_4("trident")===7){
_9.set(this.iconDivNode,"animation-name","");
}
_6.remove(this.iconDivNode,"mblVibrate");
},isOpen:function(e){
return this.paneWidget.isOpen();
},_onClick:function(e){
if(this.getParent().isEditing||e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
this.defaultClickAction(e);
},onClick:function(){
},_onNewWindowOpened:function(e){
this.set("selected",false);
},_prepareForTransition:function(e,_16){
if(_16){
this.defer(function(d){
this.set("selected",false);
},1500);
return true;
}else{
if(this.getParent().transition==="below"&&this.isOpen()){
this.close();
}else{
this.open(e);
}
return false;
}
},_closeIconClicked:function(e){
if(e){
if(e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.closeIconClicked(e)===false){
return;
}
this.defer(function(d){
this._closeIconClicked();
});
return;
}
this.close();
},closeIconClicked:function(){
},open:function(e){
var _17=this.getParent();
if(this.transition==="below"){
if(_17.single){
_17.closeAll();
}
this._open_1();
}else{
_17._opening=this;
if(_17.single){
this.paneWidget.closeHeaderNode.style.display="none";
if(!this.isOpen()){
_17.closeAll();
}
_17.appView._heading.set("label",this.label);
}
this.moveTo=_17.id+"_mblApplView";
new _c(this.domNode,this.getTransOpts(),e).dispatch();
}
},_open_1:function(){
this.paneWidget.show();
this.unhighlight();
if(this.lazy){
_e.instantiateLazyWidgets(this.containerNode,this.requires);
this.lazy=false;
}
this.scrollIntoView(this.paneWidget.domNode);
this.onOpen();
},scrollIntoView:function(_18){
var s=_f.getEnclosingScrollable(_18);
if(s){
var dim=s.getDim();
if(dim.c.h>=dim.d.h){
s.scrollIntoView(_18,true);
}
}else{
_5.global.scrollBy(0,_8.position(_18,false).y);
}
},close:function(_19){
if(!this.isOpen()){
return;
}
this.set("selected",false);
if(_4("css3-animations")&&!_19){
var _1a=this.paneWidget.domNode;
if(this.getParent().transition=="below"){
_6.add(_1a,"mblCloseContent mblShrink");
var _1b=_8.position(_1a,true);
var _1c=_8.position(this.domNode,true);
var _1d=(_1c.x+_1c.w/2-_1b.x)+"px "+(_1c.y+_1c.h/2-_1b.y)+"px";
_9.set(_1a,_10.add({},{transformOrigin:_1d}));
}else{
_6.add(_1a,"mblCloseContent mblShrink0");
}
}else{
this.paneWidget.hide();
}
this.onClose();
},onOpen:function(){
},onClose:function(){
},_setLabelAttr:function(_1e){
this.label=_1e;
var s=this._cv?this._cv(_1e):_1e;
this.labelNode.innerHTML=s;
if(this.paneWidget){
this.paneWidget.set("label",_1e);
}
},_getBadgeAttr:function(){
return this.badgeObj?this.badgeObj.getValue():null;
},_setBadgeAttr:function(_1f){
if(!this.badgeObj){
this.badgeObj=new _b({fontSize:14,className:this.badgeClass});
_9.set(this.badgeObj.domNode,{position:"absolute",top:"-2px",right:"2px"});
}
this.badgeObj.setValue(_1f);
if(_1f){
this.iconDivNode.appendChild(this.badgeObj.domNode);
}else{
this.iconDivNode.removeChild(this.badgeObj.domNode);
}
},_setDeleteIconAttr:function(_20){
if(!this.getParent()){
return;
}
this._set("deleteIcon",_20);
_20=this.deletable?_20:"";
this.deleteIconNode=_d.setIcon(_20,this.deleteIconPos,this.deleteIconNode,this.deleteIconTitle||this.alt,this.iconDivNode);
if(this.deleteIconNode){
_6.add(this.deleteIconNode,"mblIconItemDeleteIcon");
if(this.deleteIconRole){
this.deleteIconNode.setAttribute("role",this.deleteIconRole);
}
}
},_setContentAttr:function(_21){
var _22;
if(!this.paneWidget){
if(!this._tmpNode){
this._tmpNode=_7.create("div");
}
_22=this._tmpNode;
}else{
_22=this.paneWidget.containerNode;
}
if(typeof _21==="object"){
_7.empty(_22);
_22.appendChild(_21);
}else{
_22.innerHTML=_21;
}
},_setSelectedAttr:function(_23){
this.inherited(arguments);
this.iconNode&&_9.set(this.iconNode,"opacity",_23?this.getParent().pressedIconOpacity:1);
}});
return _4("dojo-bidi")?_1("dojox.mobile.IconItem",[_12,_11]):_12;
});
