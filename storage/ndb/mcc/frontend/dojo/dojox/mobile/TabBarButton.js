//>>built
define("dojox/mobile/TabBarButton",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dijit/registry","./common","./_ItemBase"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.mobile.TabBarButton",_8,{icon1:"",icon2:"",iconPos1:"",iconPos2:"",selected:false,transition:"none",tag:"LI",selectOne:true,inheritParams:function(){
if(this.icon&&!this.icon1){
this.icon1=this.icon;
}
var _9=this.getParent();
if(_9){
if(!this.transition){
this.transition=_9.transition;
}
if(this.icon1&&_9.iconBase&&_9.iconBase.charAt(_9.iconBase.length-1)==="/"){
this.icon1=_9.iconBase+this.icon1;
}
if(!this.icon1){
this.icon1=_9.iconBase;
}
if(!this.iconPos1){
this.iconPos1=_9.iconPos;
}
if(this.icon2&&_9.iconBase&&_9.iconBase.charAt(_9.iconBase.length-1)==="/"){
this.icon2=_9.iconBase+this.icon2;
}
if(!this.icon2){
this.icon2=_9.iconBase||this.icon1;
}
if(!this.iconPos2){
this.iconPos2=_9.iconPos||this.iconPos1;
}
}
},buildRendering:function(){
var a=this.anchorNode=_5.create("A",{className:"mblTabBarButtonAnchor"});
this.connect(a,"onclick","onClick");
this.box=_5.create("DIV",{className:"mblTabBarButtonTextBox"},a);
var _a=this.box;
var _b="";
var r=this.srcNodeRef;
if(r){
for(var i=0,_c=r.childNodes.length;i<_c;i++){
var n=r.firstChild;
if(n.nodeType===3){
_b+=_2.trim(n.nodeValue);
}
_a.appendChild(n);
}
}
if(!this.label){
this.label=_b;
}
this.domNode=this.srcNodeRef||_5.create(this.tag);
this.containerNode=this.domNode;
this.domNode.appendChild(a);
if(this.domNode.className.indexOf("mblDomButton")!=-1){
var _d=_5.create("DIV",null,a);
_7.createDomButton(this.domNode,null,_d);
_4.add(this.domNode,"mblTabButtonDomButton");
_4.add(_d,"mblTabButtonDomButtonClass");
}
if((this.icon1||this.icon).indexOf("mblDomButton")!=-1){
_4.add(this.domNode,"mblTabButtonDomButton");
}
},startup:function(){
if(this._started){
return;
}
this.inheritParams();
var _e=this.getParent();
var _f=_e?_e._clsName:"mblTabBarButton";
_4.add(this.domNode,_f+(this.selected?" mblTabButtonSelected":""));
if(_e&&_e.barType=="segmentedControl"){
_4.remove(this.domNode,"mblTabBarButton");
_4.add(this.domNode,_e._clsName);
this.box.className="";
}
this.set({icon1:this.icon1,icon2:this.icon2});
this.inherited(arguments);
},select:function(){
if(arguments[0]){
this.selected=false;
_4.remove(this.domNode,"mblTabButtonSelected");
}else{
this.selected=true;
_4.add(this.domNode,"mblTabButtonSelected");
for(var i=0,c=this.domNode.parentNode.childNodes;i<c.length;i++){
if(c[i].nodeType!=1){
continue;
}
var w=_6.byNode(c[i]);
if(w&&w!=this){
w.deselect();
}
}
}
if(this.iconNode1){
this.iconNode1.style.visibility=this.selected?"hidden":"";
}
if(this.iconNode2){
this.iconNode2.style.visibility=this.selected?"":"hidden";
}
},deselect:function(){
this.select(true);
},onClick:function(e){
this.defaultClickAction();
},_setIcon:function(_10,pos,num,sel){
var i="icon"+num,n="iconNode"+num,p="iconPos"+num;
if(_10){
this[i]=_10;
}
if(pos){
if(this[p]===pos){
return;
}
this[p]=pos;
}
if(_10&&_10!=="none"){
if(!this.iconDivNode){
this.iconDivNode=_5.create("DIV",{className:"mblTabBarButtonDiv"},this.anchorNode,"first");
}
if(!this[n]){
this[n]=_5.create("div",{className:"mblTabBarButtonIcon"},this.iconDivNode);
}else{
_5.empty(this[n]);
}
_7.createIcon(_10,this[p],null,this.alt,this[n]);
if(this[p]){
_4.add(this[n].firstChild,"mblTabBarButtonSpriteIcon");
}
_4.remove(this.iconDivNode,"mblTabBarButtonNoIcon");
this[n].style.visibility=sel?"hidden":"";
}else{
if(this.iconDivNode){
_4.add(this.iconDivNode,"mblTabBarButtonNoIcon");
}
}
},_setIcon1Attr:function(_11){
this._setIcon(_11,null,1,this.selected);
},_setIcon2Attr:function(_12){
this._setIcon(_12,null,2,!this.selected);
},_setIconPos1Attr:function(pos){
this._setIcon(null,pos,1,this.selected);
},_setIconPos2Attr:function(pos){
this._setIcon(null,pos,2,!this.selected);
},_setLabelAttr:function(_13){
this.label=_13;
this.box.innerHTML=this._cv?this._cv(_13):_13;
}});
});
