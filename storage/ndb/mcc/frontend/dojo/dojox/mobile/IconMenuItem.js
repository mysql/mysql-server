//>>built
define("dojox/mobile/IconMenuItem",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","./iconUtils","./_ItemBase"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.mobile.IconMenuItem",_6,{closeOnAction:false,tag:"li",baseClass:"mblIconMenuItem",selColor:"mblIconMenuItemSel",_selStartMethod:"touch",_selEndMethod:"touch",buildRendering:function(){
this.domNode=this.srcNodeRef||_4.create(this.tag);
this.inherited(arguments);
if(this.selected){
_3.add(this.domNode,this.selColor);
}
if(this.srcNodeRef){
if(!this.label){
this.label=_2.trim(this.srcNodeRef.innerHTML);
}
this.srcNodeRef.innerHTML="";
}
var a=this.anchorNode=this.containerNode=_4.create("a",{className:"mblIconMenuItemAnchor",href:"javascript:void(0)"});
var _7=_4.create("table",{className:"mblIconMenuItemTable"},a);
var _8=this.iconParentNode=_7.insertRow(-1).insertCell(-1);
this.iconNode=_4.create("div",{className:"mblIconMenuItemIcon"},_8);
this.labelNode=this.refNode=_4.create("div",{className:"mblIconMenuItemLabel"},_8);
this.position="before";
this.domNode.appendChild(a);
},startup:function(){
if(this._started){
return;
}
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
this.inherited(arguments);
if(!this._isOnLine){
this._isOnLine=true;
this.set("icon",this._pendingIcon!==undefined?this._pendingIcon:this.icon);
delete this._pendingIcon;
}
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
if(this.closeOnAction){
var p=this.getParent();
if(p&&p.hide){
p.hide();
}
}
this.defaultClickAction(e);
},onClick:function(){
},_setSelectedAttr:function(_9){
this.inherited(arguments);
_3.toggle(this.domNode,this.selColor,_9);
}});
});
