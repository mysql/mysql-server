//>>built
define("dojox/mobile/ListItem",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/has","./common","./_ItemBase","./TransitionEvent"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _3("dojox.mobile.ListItem",_9,{rightText:"",rightIcon:"",rightIcon2:"",anchorLabel:false,noArrow:false,selected:false,checked:false,arrowClass:"mblDomButtonArrow",checkClass:"mblDomButtonCheck",variableHeight:false,rightIconTitle:"",rightIcon2Title:"",btnClass:"",btnClass2:"",tag:"li",postMixInProperties:function(){
if(this.btnClass){
this.rightIcon=this.btnClass;
}
this._setBtnClassAttr=this._setRightIconAttr;
this._setBtnClass2Attr=this._setRightIcon2Attr;
},buildRendering:function(){
this.domNode=this.srcNodeRef||_6.create(this.tag);
this.inherited(arguments);
this.domNode.className="mblListItem"+(this.selected?" mblItemSelected":"");
var _b=this.box=_6.create("DIV");
_b.className="mblListItemTextBox";
if(this.anchorLabel){
_b.style.cursor="pointer";
}
var r=this.srcNodeRef;
if(r&&!this.label){
this.label="";
for(var i=0,_c=r.childNodes.length;i<_c;i++){
var n=r.firstChild;
if(n.nodeType===3&&_4.trim(n.nodeValue)!==""){
n.nodeValue=this._cv?this._cv(n.nodeValue):n.nodeValue;
this.labelNode=_6.create("SPAN",{className:"mblListItemLabel"});
this.labelNode.appendChild(n);
n=this.labelNode;
}
_b.appendChild(n);
}
}
if(!this.labelNode){
this.labelNode=_6.create("SPAN",{className:"mblListItemLabel"},_b);
}
if(this.anchorLabel){
_b.style.display="inline";
}
var a=this.anchorNode=_6.create("A");
a.className="mblListItemAnchor";
this.domNode.appendChild(a);
a.appendChild(_b);
},startup:function(){
if(this._started){
return;
}
this.inheritParams();
var _d=this.getParent();
if(this.moveTo||this.href||this.url||this.clickable||(_d&&_d.select)){
this._onClickHandle=this.connect(this.anchorNode,"onclick","onClick");
}
this.setArrow();
if(_5.contains(this.domNode,"mblVariableHeight")){
this.variableHeight=true;
}
if(this.variableHeight){
_5.add(this.domNode,"mblVariableHeight");
setTimeout(_4.hitch(this,"layoutVariableHeight"));
}
this.set("icon",this.icon);
if(!this.checked&&this.checkClass.indexOf(",")!==-1){
this.set("checked",this.checked);
}
this.inherited(arguments);
},resize:function(){
if(this.variableHeight){
this.layoutVariableHeight();
}
},onClick:function(e){
var a=e.currentTarget;
var li=a.parentNode;
if(_5.contains(li,"mblItemSelected")){
return;
}
if(this.anchorLabel){
for(var p=e.target;p.tagName!==this.tag.toUpperCase();p=p.parentNode){
if(p.className=="mblListItemTextBox"){
_5.add(p,"mblListItemTextBoxSelected");
setTimeout(function(){
_5.remove(p,"mblListItemTextBoxSelected");
},_7("android")?300:1000);
this.onAnchorLabelClicked(e);
return;
}
}
}
var _e=this.getParent();
if(_e.select){
if(_e.select==="single"){
if(!this.checked){
this.set("checked",true);
}
}else{
if(_e.select==="multiple"){
this.set("checked",!this.checked);
}
}
}
this.select();
if(this.href&&this.hrefTarget){
_8.openWindow(this.href,this.hrefTarget);
return;
}
var _f;
if(this.moveTo||this.href||this.url||this.scene){
_f={moveTo:this.moveTo,href:this.href,url:this.url,scene:this.scene,transition:this.transition,transitionDir:this.transitionDir};
}else{
if(this.transitionOptions){
_f=this.transitionOptions;
}
}
if(_f){
this.setTransitionPos(e);
return new _a(this.domNode,_f,e).dispatch();
}
},select:function(){
var _10=this.getParent();
if(_10.stateful){
_10.deselectAll();
}else{
var _11=this;
setTimeout(function(){
_11.deselect();
},_7("android")?300:1000);
}
_5.add(this.domNode,"mblItemSelected");
},deselect:function(){
_5.remove(this.domNode,"mblItemSelected");
},onAnchorLabelClicked:function(e){
},layoutVariableHeight:function(){
var h=this.anchorNode.offsetHeight;
if(h===this.anchorNodeHeight){
return;
}
this.anchorNodeHeight=h;
_1.forEach([this.rightTextNode,this.rightIcon2Node,this.rightIconNode,this.iconNode],function(n){
if(n){
var t=Math.round((h-n.offsetHeight)/2);
n.style.marginTop=t+"px";
}
});
},setArrow:function(){
if(this.checked){
return;
}
var c="";
var _12=this.getParent();
if(this.moveTo||this.href||this.url||this.clickable){
if(!this.noArrow&&!(_12&&_12.stateful)){
c=this.arrowClass;
}
}
if(c){
this._setRightIconAttr(c);
}
},_setIconAttr:function(_13){
if(!this.getParent()){
return;
}
this.icon=_13;
var a=this.anchorNode;
if(!this.iconNode){
if(_13){
var ref=this.rightIconNode||this.rightIcon2Node||this.rightTextNode||this.box;
this.iconNode=_6.create("DIV",{className:"mblListItemIcon"},ref,"before");
}
}else{
_6.empty(this.iconNode);
}
if(_13&&_13!=="none"){
_8.createIcon(_13,this.iconPos,null,this.alt,this.iconNode);
if(this.iconPos){
_5.add(this.iconNode.firstChild,"mblListItemSpriteIcon");
}
_5.remove(a,"mblListItemAnchorNoIcon");
}else{
_5.add(a,"mblListItemAnchorNoIcon");
}
},_setCheckedAttr:function(_14){
var _15=this.getParent();
if(_15&&_15.select==="single"&&_14){
_1.forEach(_15.getChildren(),function(_16){
_16.set("checked",false);
});
}
this._setRightIconAttr(this.checkClass);
var _17=this.rightIconNode.childNodes;
if(_17.length===1){
this.rightIconNode.style.display=_14?"":"none";
}else{
_17[0].style.display=_14?"":"none";
_17[1].style.display=!_14?"":"none";
}
_5.toggle(this.domNode,"mblListItemChecked",_14);
if(_15&&this.checked!==_14){
_15.onCheckStateChanged(this,_14);
}
this.checked=_14;
},_setRightTextAttr:function(_18){
if(!this.rightTextNode){
this.rightTextNode=_6.create("DIV",{className:"mblListItemRightText"},this.box,"before");
}
this.rightText=_18;
this.rightTextNode.innerHTML=this._cv?this._cv(_18):_18;
},_setRightIconAttr:function(_19){
if(!this.rightIconNode){
var ref=this.rightIcon2Node||this.rightTextNode||this.box;
this.rightIconNode=_6.create("DIV",{className:"mblListItemRightIcon"},ref,"before");
}else{
_6.empty(this.rightIconNode);
}
this.rightIcon=_19;
var arr=(_19||"").split(/,/);
if(arr.length===1){
_8.createIcon(_19,null,null,this.rightIconTitle,this.rightIconNode);
}else{
_8.createIcon(arr[0],null,null,this.rightIconTitle,this.rightIconNode);
_8.createIcon(arr[1],null,null,this.rightIconTitle,this.rightIconNode);
}
},_setRightIcon2Attr:function(_1a){
if(!this.rightIcon2Node){
var ref=this.rightTextNode||this.box;
this.rightIcon2Node=_6.create("DIV",{className:"mblListItemRightIcon2"},ref,"before");
}else{
_6.empty(this.rightIcon2Node);
}
this.rightIcon2=_1a;
_8.createIcon(_1a,null,null,this.rightIcon2Title,this.rightIcon2Node);
},_setLabelAttr:function(_1b){
this.label=_1b;
this.labelNode.innerHTML=this._cv?this._cv(_1b):_1b;
}});
});
