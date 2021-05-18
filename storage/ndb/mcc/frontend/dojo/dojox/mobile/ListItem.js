//>>built
define("dojox/mobile/ListItem",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dijit/registry","dijit/_WidgetBase","./iconUtils","./_ItemBase","./ProgressIndicator","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/ListItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
var _f=_2(_d("dojo-bidi")?"dojox.mobile.NonBidiListItem":"dojox.mobile.ListItem",_b,{rightText:"",rightIcon:"",rightIcon2:"",deleteIcon:"",anchorLabel:false,noArrow:false,checked:false,arrowClass:"",checkClass:"",uncheckClass:"",variableHeight:false,rightIconTitle:"",rightIcon2Title:"",header:false,tag:"li",busy:false,progStyle:"",layoutOnResize:false,paramsToInherit:"variableHeight,transition,deleteIcon,icon,rightIcon,rightIcon2,uncheckIcon,arrowClass,checkClass,uncheckClass,deleteIconTitle,deleteIconRole",baseClass:"mblListItem",_selStartMethod:"touch",_selEndMethod:"timer",_delayedSelection:true,_selClass:"mblListItemSelected",buildRendering:function(){
this._templated=!!this.templateString;
if(!this._templated){
this.domNode=this.containerNode=this.srcNodeRef||_5.create(this.tag);
}
this.inherited(arguments);
if(this.selected){
_4.add(this.domNode,this._selClass);
}
if(this.header){
_4.replace(this.domNode,"mblEdgeToEdgeCategory",this.baseClass);
}
if(!this._templated){
this.labelNode=_5.create("div",{className:"mblListItemLabel"});
var ref=this.srcNodeRef;
if(ref&&ref.childNodes.length===1&&ref.firstChild.nodeType===3){
this.labelNode.appendChild(ref.firstChild);
}
this.domNode.appendChild(this.labelNode);
}
this._layoutChildren=[];
},startup:function(){
if(this._started){
return;
}
var _10=this.getParent();
var _11=this.getTransOpts();
if((!this._templated||this.labelNode)&&this.anchorLabel){
this.labelNode.style.display="inline";
this.labelNode.style.cursor="pointer";
this.connect(this.labelNode,"onclick","_onClick");
this.onTouchStart=function(e){
return (e.target!==this.labelNode);
};
}
this.inherited(arguments);
if(_4.contains(this.domNode,"mblVariableHeight")){
this.variableHeight=true;
}
if(this.variableHeight){
_4.add(this.domNode,"mblVariableHeight");
this.defer("layoutVariableHeight");
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
if(_10&&_10.select){
this.set("checked",this._pendingChecked!==undefined?this._pendingChecked:this.checked);
_7.set(this.domNode,"role","option");
if(this._pendingChecked||this.checked){
_7.set(this.domNode,"aria-selected","true");
}
delete this._pendingChecked;
}
this.setArrow();
this.layoutChildren();
},_updateHandles:function(){
var _12=this.getParent();
var _13=this.getTransOpts();
if(_13.moveTo||_13.href||_13.url||this.clickable||(_12&&_12.select)){
if(!this._keydownHandle){
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
}
this._handleClick=true;
}else{
if(this._keydownHandle){
this.disconnect(this._keydownHandle);
this._keydownHandle=null;
}
this._handleClick=false;
}
this.inherited(arguments);
},layoutChildren:function(){
var _14;
_1.forEach(this.domNode.childNodes,function(n){
if(n.nodeType!==1){
return;
}
var _15=n.getAttribute("layout")||n.getAttribute("data-mobile-layout")||(_8.byNode(n)||{}).layout;
if(_15){
_4.add(n,"mblListItemLayout"+_15.charAt(0).toUpperCase()+_15.substring(1));
this._layoutChildren.push(n);
if(_15==="center"){
_14=n;
}
}
},this);
if(_14){
this.domNode.insertBefore(_14,this.domNode.firstChild);
}
},resize:function(){
if(this.layoutOnResize&&this.variableHeight){
this.layoutVariableHeight();
}
if(!this._templated||this.labelNode){
this.labelNode.style.display=this.labelNode.firstChild?"block":"inline";
}
},_onTouchStart:function(e){
if(e.target.getAttribute("preventTouch")||e.target.getAttribute("data-mobile-prevent-touch")||(_8.getEnclosingWidget(e.target)||{}).preventTouch){
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
if((this._templated||n)&&this.anchorLabel&&e.currentTarget===n){
_4.add(n,"mblListItemLabelSelected");
this.defer(function(){
_4.remove(n,"mblListItemLabelSelected");
},this._duration);
this.onAnchorLabelClicked(e);
return;
}
var _16=this.getParent();
if(_16.select){
if(_16.select==="single"){
if(!this.checked){
this.set("checked",true);
}
}else{
if(_16.select==="multiple"){
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
var _17=this.domNode;
var f=function(){
var t=Math.round((_17.offsetHeight-n.offsetHeight)/2)-_6.get(_17,"paddingTop");
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
var _18=this.getParent();
var _19=this.getTransOpts();
if(_19.moveTo||_19.href||_19.url||this.clickable){
if(!this.noArrow&&!(_18&&_18.selectOne)){
c=this.arrowClass||"mblDomButtonArrow";
_7.set(this.domNode,"role","button");
}
}
if(c){
this._setRightIconAttr(c);
}
},_findRef:function(_1a){
var i,_1b,_1c=["deleteIcon","icon","rightIcon","uncheckIcon","rightIcon2","rightText"];
for(i=_1.indexOf(_1c,_1a)+1;i<_1c.length;i++){
_1b=this[_1c[i]+"Node"];
if(_1b){
return _1b;
}
}
for(i=_1c.length-1;i>=0;i--){
_1b=this[_1c[i]+"Node"];
if(_1b){
return _1b.nextSibling;
}
}
return this.domNode.firstChild;
},_setIcon:function(_1d,_1e){
if(!this._isOnLine){
this["_pending_"+_1e]=_1d;
return;
}
this._set(_1e,_1d);
this[_1e+"Node"]=_a.setIcon(_1d,this[_1e+"Pos"],this[_1e+"Node"],this[_1e+"Title"]||this.alt,this.domNode,this._findRef(_1e),"before");
if(this[_1e+"Node"]){
var cap=_1e.charAt(0).toUpperCase()+_1e.substring(1);
_4.add(this[_1e+"Node"],"mblListItem"+cap);
}
var _1f=this[_1e+"Role"];
if(_1f){
this[_1e+"Node"].setAttribute("role",_1f);
}
},_setDeleteIconAttr:function(_20){
this._setIcon(_20,"deleteIcon");
},_setIconAttr:function(_21){
this._setIcon(_21,"icon");
},_setRightTextAttr:function(_22){
if(!this._templated&&!this.rightTextNode){
this.rightTextNode=_5.create("div",{className:"mblListItemRightText"},this.labelNode,"before");
}
this.rightText=_22;
this.rightTextNode.innerHTML=this._cv?this._cv(_22):_22;
},_setRightIconAttr:function(_23){
this._setIcon(_23,"rightIcon");
},_setUncheckIconAttr:function(_24){
this._setIcon(_24,"uncheckIcon");
},_setRightIcon2Attr:function(_25){
this._setIcon(_25,"rightIcon2");
},_setCheckedAttr:function(_26){
if(!this._isOnLine){
this._pendingChecked=_26;
return;
}
var _27=this.getParent();
if(_27&&_27.select==="single"&&_26){
_1.forEach(_27.getChildren(),function(_28){
_28!==this&&_28.checked&&_28.set("checked",false)&&_7.set(_28.domNode,"aria-selected","false");
},this);
}
this._setRightIconAttr(this.checkClass||"mblDomButtonCheck");
this._setUncheckIconAttr(this.uncheckClass);
_4.toggle(this.domNode,"mblListItemChecked",_26);
_4.toggle(this.domNode,"mblListItemUnchecked",!_26);
_4.toggle(this.domNode,"mblListItemHasUncheck",!!this.uncheckIconNode);
this.rightIconNode.style.position=(this.uncheckIconNode&&!_26)?"absolute":"";
if(_27&&this.checked!==_26){
_27.onCheckStateChanged(this,_26);
}
this._set("checked",_26);
_7.set(this.domNode,"aria-selected",_26?"true":"false");
},_setBusyAttr:function(_29){
var _2a=this._prog;
if(_29){
if(!this._progNode){
this._progNode=_5.create("div",{className:"mblListItemIcon"});
_2a=this._prog=new _c({size:25,center:false,removeOnStop:false});
_4.add(_2a.domNode,this.progStyle);
this._progNode.appendChild(_2a.domNode);
}
if(this.iconNode){
this.domNode.replaceChild(this._progNode,this.iconNode);
}else{
_5.place(this._progNode,this._findRef("icon"),"before");
}
_2a.start();
}else{
if(this._progNode){
if(this.iconNode){
this.domNode.replaceChild(this.iconNode,this._progNode);
}else{
this.domNode.removeChild(this._progNode);
}
_2a.stop();
}
}
this._set("busy",_29);
},_setSelectedAttr:function(_2b){
this.inherited(arguments);
_4.toggle(this.domNode,this._selClass,_2b);
},_setClickableAttr:function(_2c){
this._set("clickable",_2c);
this._updateHandles();
},_setMoveToAttr:function(_2d){
this._set("moveTo",_2d);
this._updateHandles();
},_setHrefAttr:function(_2e){
this._set("href",_2e);
this._updateHandles();
},_setUrlAttr:function(url){
this._set("url",url);
this._updateHandles();
}});
_f.ChildWidgetProperties={layout:"",preventTouch:false};
_3.extend(_9,_f.ChildWidgetProperties);
return _d("dojo-bidi")?_2("dojox.mobile.ListItem",[_f,_e]):_f;
});
