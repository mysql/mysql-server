//>>built
define("dojox/mobile/ListItem",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_WidgetBase","./iconUtils","./_ItemBase","./ProgressIndicator"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_2("dojox.mobile.ListItem",_a,{rightText:"",rightIcon:"",rightIcon2:"",deleteIcon:"",anchorLabel:false,noArrow:false,checked:false,arrowClass:"",checkClass:"",uncheckClass:"",variableHeight:false,rightIconTitle:"",rightIcon2Title:"",header:false,tag:"li",busy:false,progStyle:"",paramsToInherit:"variableHeight,transition,deleteIcon,icon,rightIcon,rightIcon2,uncheckIcon,arrowClass,checkClass,uncheckClass,deleteIconTitle,deleteIconRole",baseClass:"mblListItem",_selStartMethod:"touch",_selEndMethod:"timer",_delayedSelection:true,_selClass:"mblListItemSelected",buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_5.create(this.tag);
this.inherited(arguments);
if(this.selected){
_4.add(this.domNode,this._selClass);
}
if(this.header){
_4.replace(this.domNode,"mblEdgeToEdgeCategory",this.baseClass);
}
this.labelNode=_5.create("div",{className:"mblListItemLabel"});
var _d=this.srcNodeRef;
if(_d&&_d.childNodes.length===1&&_d.firstChild.nodeType===3){
this.labelNode.appendChild(_d.firstChild);
}
this.domNode.appendChild(this.labelNode);
if(this.anchorLabel){
this.labelNode.style.display="inline";
this.labelNode.style.cursor="pointer";
this._anchorClickHandle=this.connect(this.labelNode,"onclick","_onClick");
this.onTouchStart=function(e){
return (e.target!==this.labelNode);
};
}
this._layoutChildren=[];
},startup:function(){
if(this._started){
return;
}
var _e=this.getParent();
var _f=this.getTransOpts();
if(_f.moveTo||_f.href||_f.url||this.clickable||(_e&&_e.select)){
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
}else{
this._handleClick=false;
}
this.inherited(arguments);
if(_4.contains(this.domNode,"mblVariableHeight")){
this.variableHeight=true;
}
if(this.variableHeight){
_4.add(this.domNode,"mblVariableHeight");
this.defer(_3.hitch(this,"layoutVariableHeight"),0);
}
if(!this._isOnLine){
this._isOnLine=true;
this.set({icon:this._pending_icon!==undefined?this._pending_icon:this.icon,deleteIcon:this._pending_deleteIcon!==undefined?this._pending_deleteIcon:this.deleteIcon,rightIcon:this._pending_rightIcon!==undefined?this._pending_rightIcon:this.rightIcon,rightIcon2:this._pending_rightIcon2!==undefined?this._pending_rightIcon2:this.rightIcon2,uncheckIcon:this._pending_uncheckIcon!==undefined?this._pending_uncheckIcon:this.uncheckIcon});
delete this._pending_icon;
delete this._pending_deleteIcon;
delete this._pending_rightIcon;
delete this._pending_rightIcon2;
delete this._pending_uncheckIcon;
}
if(_e&&_e.select){
this.set("checked",this._pendingChecked!==undefined?this._pendingChecked:this.checked);
delete this._pendingChecked;
}
this.setArrow();
this.layoutChildren();
},layoutChildren:function(){
var _10;
_1.forEach(this.domNode.childNodes,function(n){
if(n.nodeType!==1){
return;
}
var _11=n.getAttribute("layout")||(_7.byNode(n)||{}).layout;
if(_11){
_4.add(n,"mblListItemLayout"+_11.charAt(0).toUpperCase()+_11.substring(1));
this._layoutChildren.push(n);
if(_11==="center"){
_10=n;
}
}
},this);
if(_10){
this.domNode.insertBefore(_10,this.domNode.firstChild);
}
},resize:function(){
if(this.variableHeight){
this.layoutVariableHeight();
}
this.labelNode.style.display=this.labelNode.firstChild?"block":"inline";
},_onTouchStart:function(e){
if(e.target.getAttribute("preventTouch")||(_7.getEnclosingWidget(e.target)||{}).preventTouch){
return;
}
this.inherited(arguments);
},_onClick:function(e){
if(this.getParent().isEditing||e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
var n=this.labelNode;
if(this.anchorLabel&&e.currentTarget===n){
_4.add(n,"mblListItemLabelSelected");
setTimeout(function(){
_4.remove(n,"mblListItemLabelSelected");
},this._duration);
this.onAnchorLabelClicked(e);
return;
}
var _12=this.getParent();
if(_12.select){
if(_12.select==="single"){
if(!this.checked){
this.set("checked",true);
}
}else{
if(_12.select==="multiple"){
this.set("checked",!this.checked);
}
}
}
this.defaultClickAction(e);
},onClick:function(){
},onAnchorLabelClicked:function(e){
},layoutVariableHeight:function(){
var h=this.domNode.offsetHeight;
if(h===this.domNodeHeight){
return;
}
this.domNodeHeight=h;
_1.forEach(this._layoutChildren.concat([this.rightTextNode,this.rightIcon2Node,this.rightIconNode,this.uncheckIconNode,this.iconNode,this.deleteIconNode,this.knobIconNode]),function(n){
if(n){
var _13=this.domNode;
var f=function(){
var t=Math.round((_13.offsetHeight-n.offsetHeight)/2)-_6.get(_13,"paddingTop");
n.style.marginTop=t+"px";
};
if(n.offsetHeight===0&&n.tagName==="IMG"){
n.onload=f;
}else{
f();
}
}
},this);
},setArrow:function(){
if(this.checked){
return;
}
var c="";
var _14=this.getParent();
var _15=this.getTransOpts();
if(_15.moveTo||_15.href||_15.url||this.clickable){
if(!this.noArrow&&!(_14&&_14.selectOne)){
c=this.arrowClass||"mblDomButtonArrow";
}
}
if(c){
this._setRightIconAttr(c);
}
},_findRef:function(_16){
var i,_17,_18=["deleteIcon","icon","rightIcon","uncheckIcon","rightIcon2","rightText"];
for(i=_1.indexOf(_18,_16)+1;i<_18.length;i++){
_17=this[_18[i]+"Node"];
if(_17){
return _17;
}
}
for(i=_18.length-1;i>=0;i--){
_17=this[_18[i]+"Node"];
if(_17){
return _17.nextSibling;
}
}
return this.domNode.firstChild;
},_setIcon:function(_19,_1a){
if(!this._isOnLine){
this["_pending_"+_1a]=_19;
return;
}
this._set(_1a,_19);
this[_1a+"Node"]=_9.setIcon(_19,this[_1a+"Pos"],this[_1a+"Node"],this[_1a+"Title"]||this.alt,this.domNode,this._findRef(_1a),"before");
if(this[_1a+"Node"]){
var cap=_1a.charAt(0).toUpperCase()+_1a.substring(1);
_4.add(this[_1a+"Node"],"mblListItem"+cap);
}
var _1b=this[_1a+"Role"];
if(_1b){
this[_1a+"Node"].setAttribute("role",_1b);
}
},_setDeleteIconAttr:function(_1c){
this._setIcon(_1c,"deleteIcon");
},_setIconAttr:function(_1d){
this._setIcon(_1d,"icon");
},_setRightTextAttr:function(_1e){
if(!this.rightTextNode){
this.rightTextNode=_5.create("div",{className:"mblListItemRightText"},this.labelNode,"before");
}
this.rightText=_1e;
this.rightTextNode.innerHTML=this._cv?this._cv(_1e):_1e;
},_setRightIconAttr:function(_1f){
this._setIcon(_1f,"rightIcon");
},_setUncheckIconAttr:function(_20){
this._setIcon(_20,"uncheckIcon");
},_setRightIcon2Attr:function(_21){
this._setIcon(_21,"rightIcon2");
},_setCheckedAttr:function(_22){
if(!this._isOnLine){
this._pendingChecked=_22;
return;
}
var _23=this.getParent();
if(_23&&_23.select==="single"&&_22){
_1.forEach(_23.getChildren(),function(_24){
_24!==this&&_24.checked&&_24.set("checked",false);
},this);
}
this._setRightIconAttr(this.checkClass||"mblDomButtonCheck");
this._setUncheckIconAttr(this.uncheckClass);
_4.toggle(this.domNode,"mblListItemChecked",_22);
_4.toggle(this.domNode,"mblListItemUnchecked",!_22);
_4.toggle(this.domNode,"mblListItemHasUncheck",!!this.uncheckIconNode);
this.rightIconNode.style.position=(this.uncheckIconNode&&!_22)?"absolute":"";
if(_23&&this.checked!==_22){
_23.onCheckStateChanged(this,_22);
}
this._set("checked",_22);
},_setBusyAttr:function(_25){
var _26=this._prog;
if(_25){
if(!this._progNode){
this._progNode=_5.create("div",{className:"mblListItemIcon"});
_26=this._prog=new _b({size:25,center:false});
_4.add(_26.domNode,this.progStyle);
this._progNode.appendChild(_26.domNode);
}
if(this.iconNode){
this.domNode.replaceChild(this._progNode,this.iconNode);
}else{
_5.place(this._progNode,this._findRef("icon"),"before");
}
_26.start();
}else{
if(this.iconNode){
this.domNode.replaceChild(this.iconNode,this._progNode);
}else{
this.domNode.removeChild(this._progNode);
}
_26.stop();
}
this._set("busy",_25);
},_setSelectedAttr:function(_27){
this.inherited(arguments);
_4.toggle(this.domNode,this._selClass,_27);
}});
_c.ChildWidgetProperties={layout:"",preventTouch:false};
_3.extend(_8,_c.ChildWidgetProperties);
return _c;
});
