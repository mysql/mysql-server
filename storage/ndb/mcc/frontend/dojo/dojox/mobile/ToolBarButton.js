//>>built
define("dojox/mobile/ToolBarButton",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","./sniff","./_ItemBase","dojo/has!dojo-bidi?dojox/mobile/bidi/ToolBarButton"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_1(_8("dojo-bidi")?"dojox.mobile.NonBidiToolBarButton":"dojox.mobile.ToolBarButton",_9,{selected:false,arrow:"",light:true,defaultColor:"mblColorDefault",selColor:"mblColorDefaultSel",baseClass:"mblToolBarButton",_selStartMethod:"touch",_selEndMethod:"touch",buildRendering:function(){
if(!this.label&&this.srcNodeRef){
this.label=this.srcNodeRef.innerHTML;
}
this.label=_2.trim(this.label);
this.domNode=(this.srcNodeRef&&this.srcNodeRef.tagName==="SPAN")?this.srcNodeRef:_5.create("span");
_7.set(this.domNode,"role","button");
this.inherited(arguments);
if(this.light&&!this.arrow&&(!this.icon||!this.label)){
this.labelNode=this.tableNode=this.bodyNode=this.iconParentNode=this.domNode;
_4.add(this.domNode,this.defaultColor+" mblToolBarButtonBody"+(this.icon?" mblToolBarButtonLightIcon":" mblToolBarButtonLightText"));
return;
}
this.domNode.innerHTML="";
if(this.arrow==="left"||this.arrow==="right"){
this.arrowNode=_5.create("span",{className:"mblToolBarButtonArrow mblToolBarButton"+(this.arrow==="left"?"Left":"Right")+"Arrow "+(_8("ie")<10?"":(this.defaultColor+" "+this.defaultColor+"45"))},this.domNode);
_4.add(this.domNode,"mblToolBarButtonHas"+(this.arrow==="left"?"Left":"Right")+"Arrow");
}
this.bodyNode=_5.create("span",{className:"mblToolBarButtonBody"},this.domNode);
this.tableNode=_5.create("table",{cellPadding:"0",cellSpacing:"0",border:"0",role:"presentation"},this.bodyNode);
if(!this.label&&this.arrow){
this.tableNode.className="mblToolBarButtonText";
}
var _c=this.tableNode.insertRow(-1);
this.iconParentNode=_c.insertCell(-1);
this.labelNode=_c.insertCell(-1);
this.iconParentNode.className="mblToolBarButtonIcon";
this.labelNode.className="mblToolBarButtonLabel";
if(this.icon&&this.icon!=="none"&&this.label){
_4.add(this.domNode,"mblToolBarButtonHasIcon");
_4.add(this.bodyNode,"mblToolBarButtonLabeledIcon");
}
_4.add(this.bodyNode,this.defaultColor);
},startup:function(){
if(this._started){
return;
}
this.connect(this.domNode,"onkeydown","_onClick");
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
this.defaultClickAction(e);
},onClick:function(){
},_setLabelAttr:function(_d){
this.inherited(arguments);
_4.toggle(this.tableNode,"mblToolBarButtonText",_d||this.arrow);
},_setSelectedAttr:function(_e){
var _f=function(_10,a,b){
_4.replace(_10,a+" "+a+"45",b+" "+b+"45");
};
this.inherited(arguments);
if(_e){
_4.replace(this.bodyNode,this.selColor,this.defaultColor);
if(!(_8("ie")<10)&&this.arrowNode){
_f(this.arrowNode,this.selColor,this.defaultColor);
}
}else{
_4.replace(this.bodyNode,this.defaultColor,this.selColor);
if(!(_8("ie")<10)&&this.arrowNode){
_f(this.arrowNode,this.defaultColor,this.selColor);
}
}
_4.toggle(this.domNode,"mblToolBarButtonSelected",_e);
_4.toggle(this.bodyNode,"mblToolBarButtonBodySelected",_e);
}});
return _8("dojo-bidi")?_1("dojox.mobile.ToolBarButton",[_b,_a]):_b;
});
