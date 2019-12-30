//>>built
define("dojox/mobile/IconItem",["dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","./_ItemBase","./Badge","./TransitionEvent","./iconUtils","./lazyLoadUtils","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
return _1("dojox.mobile.IconItem",_a,{lazy:false,requires:"",timeout:10,content:"",badge:"",badgeClass:"mblDomButtonRedBadge",deletable:true,deleteIcon:"",tag:"li",paramsToInherit:"transition,icon,deleteIcon,badgeClass,deleteIconTitle,deleteIconRole",baseClass:"mblIconItem",_selStartMethod:"touch",_selEndMethod:"none",destroy:function(){
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
require([p.iconItemPaneClass],_3.hitch(this,function(_10){
var w=this.paneWidget=new _10(p.iconItemPaneProps);
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
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
},highlight:function(_11){
_6.add(this.iconDivNode,"mblVibrate");
_11=(_11!==undefined)?_11:this.timeout;
if(_11>0){
var _12=this;
setTimeout(function(){
_12.unhighlight();
},_11*1000);
}
},unhighlight:function(){
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
},_prepareForTransition:function(e,_13){
if(_13){
setTimeout(_3.hitch(this,function(d){
this.set("selected",false);
}),1500);
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
setTimeout(_3.hitch(this,function(d){
this._closeIconClicked();
}),0);
return;
}
this.close();
},closeIconClicked:function(){
},open:function(e){
var _14=this.getParent();
if(this.transition==="below"){
if(_14.single){
_14.closeAll();
}
this._open_1();
}else{
_14._opening=this;
if(_14.single){
this.paneWidget.closeHeaderNode.style.display="none";
if(!this.isOpen()){
_14.closeAll();
}
_14.appView._heading.set("label",this.label);
}
this.moveTo=_14.id+"_mblApplView";
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
},scrollIntoView:function(_15){
var s=_f.getEnclosingScrollable(_15);
if(s){
s.scrollIntoView(_15,true);
}else{
_5.global.scrollBy(0,_8.position(_15,false).y);
}
},close:function(_16){
if(!this.isOpen()){
return;
}
this.set("selected",false);
if(_4("webkit")&&!_16){
var _17=this.paneWidget.domNode;
if(this.getParent().transition=="below"){
_6.add(_17,"mblCloseContent mblShrink");
var _18=_8.position(_17,true);
var _19=_8.position(this.domNode,true);
var _1a=(_19.x+_19.w/2-_18.x)+"px "+(_19.y+_19.h/2-_18.y)+"px";
_9.set(_17,{webkitTransformOrigin:_1a});
}else{
_6.add(_17,"mblCloseContent mblShrink0");
}
}else{
this.paneWidget.hide();
}
this.onClose();
},onOpen:function(){
},onClose:function(){
},_setLabelAttr:function(_1b){
this.label=_1b;
var s=this._cv?this._cv(_1b):_1b;
this.labelNode.innerHTML=s;
if(this.paneWidget){
this.paneWidget.set("label",_1b);
}
},_getBadgeAttr:function(){
return this.badgeObj?this.badgeObj.getValue():null;
},_setBadgeAttr:function(_1c){
if(!this.badgeObj){
this.badgeObj=new _b({fontSize:14,className:this.badgeClass});
_9.set(this.badgeObj.domNode,{position:"absolute",top:"-2px",right:"2px"});
}
this.badgeObj.setValue(_1c);
if(_1c){
this.iconDivNode.appendChild(this.badgeObj.domNode);
}else{
this.iconDivNode.removeChild(this.badgeObj.domNode);
}
},_setDeleteIconAttr:function(_1d){
if(!this.getParent()){
return;
}
this._set("deleteIcon",_1d);
_1d=this.deletable?_1d:"";
this.deleteIconNode=_d.setIcon(_1d,this.deleteIconPos,this.deleteIconNode,this.deleteIconTitle||this.alt,this.iconDivNode);
if(this.deleteIconNode){
_6.add(this.deleteIconNode,"mblIconItemDeleteIcon");
if(this.deleteIconRole){
this.deleteIconNode.setAttribute("role",this.deleteIconRole);
}
}
},_setContentAttr:function(_1e){
var _1f;
if(!this.paneWidget){
if(!this._tmpNode){
this._tmpNode=_7.create("div");
}
_1f=this._tmpNode;
}else{
_1f=this.paneWidget.containerNode;
}
if(typeof _1e==="object"){
_7.empty(_1f);
_1f.appendChild(_1e);
}else{
_1f.innerHTML=_1e;
}
},_setSelectedAttr:function(_20){
this.inherited(arguments);
this.iconNode&&_9.set(this.iconNode,"opacity",_20?this.getParent().pressedIconOpacity:1);
}});
});
