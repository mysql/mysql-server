//>>built
require({cache:{"url:dijit/form/templates/Select.html":"<table class=\"dijit dijitReset dijitInline dijitLeft\"\n\tdata-dojo-attach-point=\"_buttonNode,tableNode,focusNode\" cellspacing='0' cellpadding='0'\n\trole=\"combobox\" aria-haspopup=\"true\"\n\t><tbody role=\"presentation\"><tr role=\"presentation\"\n\t\t><td class=\"dijitReset dijitStretch dijitButtonContents dijitButtonNode\" role=\"presentation\"\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"  data-dojo-attach-point=\"containerNode,_popupStateNode\"></span\n\t\t\t><input type=\"hidden\" ${!nameAttrSetting} data-dojo-attach-point=\"valueNode\" value=\"${value}\" aria-hidden=\"true\"\n\t\t/></td><td class=\"dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton\"\n\t\t\t\tdata-dojo-attach-point=\"titleNode\" role=\"presentation\"\n\t\t\t><div class=\"dijitReset dijitArrowButtonInner\" role=\"presentation\"></div\n\t\t\t><div class=\"dijitReset dijitArrowButtonChar\" role=\"presentation\">&#9660;</div\n\t\t></td\n\t></tr></tbody\n></table>\n"}});
define("dijit/form/Select",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/_base/event","dojo/i18n","dojo/_base/lang","./_FormSelectWidget","../_HasDropDown","../Menu","../MenuItem","../MenuSeparator","../Tooltip","dojo/text!./templates/Select.html","dojo/i18n!./nls/validate"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2("dijit.form._SelectMenu",_c,{buildRendering:function(){
this.inherited(arguments);
var o=(this.menuTableNode=this.domNode);
var n=(this.domNode=_5.create("div",{style:{overflowX:"hidden",overflowY:"scroll"}}));
if(o.parentNode){
o.parentNode.replaceChild(n,o);
}
_4.remove(o,"dijitMenuTable");
n.className=o.className+" dijitSelectMenu";
o.className="dijitReset dijitMenuTable";
o.setAttribute("role","listbox");
n.setAttribute("role","presentation");
n.appendChild(o);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmousemove",_7.stop);
},resize:function(mb){
if(mb){
_6.setMarginBox(this.domNode,mb);
if("w" in mb){
this.menuTableNode.style.width="100%";
}
}
}});
var _12=_2("dijit.form.Select",[_a,_b],{baseClass:"dijitSelect",templateString:_10,required:false,state:"",message:"",tooltipPosition:[],emptyLabel:"&#160;",_isLoaded:false,_childrenLoaded:false,_fillContent:function(){
this.inherited(arguments);
if(this.options.length&&!this.value&&this.srcNodeRef){
var si=this.srcNodeRef.selectedIndex||0;
this.value=this.options[si>=0?si:0].value;
}
this.dropDown=new _11({id:this.id+"_menu"});
_4.add(this.dropDown.domNode,this.baseClass+"Menu");
},_getMenuItemForOption:function(_13){
if(!_13.value&&!_13.label){
return new _e();
}else{
var _14=_9.hitch(this,"_setValueAttr",_13);
var _15=new _d({option:_13,label:_13.label||this.emptyLabel,onClick:_14,disabled:_13.disabled||false});
_15.focusNode.setAttribute("role","listitem");
return _15;
}
},_addOptionItem:function(_16){
if(this.dropDown){
this.dropDown.addChild(this._getMenuItemForOption(_16));
}
},_getChildren:function(){
if(!this.dropDown){
return [];
}
return this.dropDown.getChildren();
},_loadChildren:function(_17){
if(_17===true){
if(this.dropDown){
delete this.dropDown.focusedChild;
}
if(this.options.length){
this.inherited(arguments);
}else{
_1.forEach(this._getChildren(),function(_18){
_18.destroyRecursive();
});
var _19=new _d({label:"&#160;"});
this.dropDown.addChild(_19);
}
}else{
this._updateSelection();
}
this._isLoaded=false;
this._childrenLoaded=true;
if(!this._loadingStore){
this._setValueAttr(this.value);
}
},_setValueAttr:function(_1a){
this.inherited(arguments);
_3.set(this.valueNode,"value",this.get("value"));
this.validate(this.focused);
},_setDisabledAttr:function(_1b){
this.inherited(arguments);
this.validate(this.focused);
},_setRequiredAttr:function(_1c){
this._set("required",_1c);
this.focusNode.setAttribute("aria-required",_1c);
this.validate(this.focused);
},_setDisplay:function(_1d){
var lbl=_1d||this.emptyLabel;
this.containerNode.innerHTML="<span class=\"dijitReset dijitInline "+this.baseClass+"Label\">"+lbl+"</span>";
this.focusNode.setAttribute("aria-valuetext",lbl);
},validate:function(_1e){
var _1f=this.disabled||this.isValid(_1e);
this._set("state",_1f?"":"Incomplete");
this.focusNode.setAttribute("aria-invalid",_1f?"false":"true");
var _20=_1f?"":this._missingMsg;
if(_20&&this.focused&&this._hasBeenBlurred){
_f.show(_20,this.domNode,this.tooltipPosition,!this.isLeftToRight());
}else{
_f.hide(this.domNode);
}
this._set("message",_20);
return _1f;
},isValid:function(){
return (!this.required||this.value===0||!(/^\s*$/.test(this.value||"")));
},reset:function(){
this.inherited(arguments);
_f.hide(this.domNode);
this.validate(this.focused);
},postMixInProperties:function(){
this.inherited(arguments);
this._missingMsg=_8.getLocalization("dijit.form","validate",this.lang).missingMessage;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmousemove",_7.stop);
},_setStyleAttr:function(_21){
this.inherited(arguments);
_4.toggle(this.domNode,this.baseClass+"FixedWidth",!!this.domNode.style.width);
},isLoaded:function(){
return this._isLoaded;
},loadDropDown:function(_22){
this._loadChildren(true);
this._isLoaded=true;
_22();
},closeDropDown:function(){
this.inherited(arguments);
if(this.dropDown&&this.dropDown.menuTableNode){
this.dropDown.menuTableNode.style.width="";
}
},uninitialize:function(_23){
if(this.dropDown&&!this.dropDown._destroyed){
this.dropDown.destroyRecursive(_23);
delete this.dropDown;
}
this.inherited(arguments);
},_onFocus:function(){
this.validate(true);
this.inherited(arguments);
},_onBlur:function(){
_f.hide(this.domNode);
this.inherited(arguments);
}});
_12._Menu=_11;
return _12;
});
