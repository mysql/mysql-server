//>>built
require({cache:{"url:dijit/form/templates/Select.html":"<table class=\"dijit dijitReset dijitInline dijitLeft\"\n\tdata-dojo-attach-point=\"_buttonNode,tableNode,focusNode,_popupStateNode\" cellspacing='0' cellpadding='0'\n\trole=\"listbox\" aria-haspopup=\"true\"\n\t><tbody role=\"presentation\"><tr role=\"presentation\"\n\t\t><td class=\"dijitReset dijitStretch dijitButtonContents\" role=\"presentation\"\n\t\t\t><div class=\"dijitReset dijitInputField dijitButtonText\"  data-dojo-attach-point=\"containerNode\" role=\"presentation\"></div\n\t\t\t><div class=\"dijitReset dijitValidationContainer\"\n\t\t\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t/></div\n\t\t\t><input type=\"hidden\" ${!nameAttrSetting} data-dojo-attach-point=\"valueNode\" value=\"${value}\" aria-hidden=\"true\"\n\t\t/></td\n\t\t><td class=\"dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton dijitArrowButtonContainer\"\n\t\t\tdata-dojo-attach-point=\"titleNode\" role=\"presentation\"\n\t\t\t><input class=\"dijitReset dijitInputField dijitArrowButtonInner\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t\t${_buttonInputDisabled}\n\t\t/></td\n\t></tr></tbody\n></table>\n"}});
define("dijit/form/Select",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/_base/event","dojo/i18n","dojo/_base/lang","dojo/sniff","./_FormSelectWidget","../_HasDropDown","../Menu","../MenuItem","../MenuSeparator","../Tooltip","dojo/text!./templates/Select.html","dojo/i18n!./nls/validate"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2("dijit.form._SelectMenu",_c,{autoFocus:true,buildRendering:function(){
this.inherited(arguments);
var o=(this.menuTableNode=this.domNode);
var n=(this.domNode=this.ownerDocument.createElement("div"));
n.style.cssText="overflow-x: hidden; overflow-y: scroll";
if(o.parentNode){
o.parentNode.replaceChild(n,o);
}
_4.remove(o,"dijitMenuTable");
n.className=o.className+" dijitSelectMenu";
o.className="dijitReset dijitMenuTable";
n.setAttribute("role","listbox");
o.setAttribute("role","presentation");
n.appendChild(o);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onselectstart",_6.stop);
},focus:function(){
var _12=false,val=this.parentWidget.value;
if(_8.isArray(val)){
val=val[val.length-1];
}
if(val){
_1.forEach(this.parentWidget._getChildren(),function(_13){
if(_13.option&&(val===_13.option.value)){
_12=true;
this.focusChild(_13,false);
}
},this);
}
if(!_12){
this.inherited(arguments);
}
},resize:function(mb){
if(mb){
_5.setMarginBox(this.domNode,mb);
if("w" in mb){
this.menuTableNode.style.width="100%";
}
}
}});
var _14=_2("dijit.form.Select",[_a,_b],{baseClass:"dijitSelect dijitValidationTextBox",templateString:_10,_buttonInputDisabled:_9("ie")?"disabled":"",required:false,state:"",message:"",tooltipPosition:[],emptyLabel:"&#160;",_isLoaded:false,_childrenLoaded:false,_fillContent:function(){
this.inherited(arguments);
if(this.options.length&&!this.value&&this.srcNodeRef){
var si=this.srcNodeRef.selectedIndex||0;
this.value=this.options[si>=0?si:0].value;
}
this.dropDown=new _11({id:this.id+"_menu",parentWidget:this});
_4.add(this.dropDown.domNode,this.baseClass.replace(/\s+|$/g,"Menu "));
},_getMenuItemForOption:function(_15){
if(!_15.value&&!_15.label){
return new _e({ownerDocument:this.ownerDocument});
}else{
var _16=_8.hitch(this,"_setValueAttr",_15);
var _17=new _d({option:_15,label:_15.label||this.emptyLabel,onClick:_16,ownerDocument:this.ownerDocument,dir:this.dir,disabled:_15.disabled||false});
_17.focusNode.setAttribute("role","option");
return _17;
}
},_addOptionItem:function(_18){
if(this.dropDown){
this.dropDown.addChild(this._getMenuItemForOption(_18));
}
},_getChildren:function(){
if(!this.dropDown){
return [];
}
return this.dropDown.getChildren();
},_loadChildren:function(_19){
if(_19===true){
if(this.dropDown){
delete this.dropDown.focusedChild;
}
if(this.options.length){
this.inherited(arguments);
}else{
_1.forEach(this._getChildren(),function(_1a){
_1a.destroyRecursive();
});
var _1b=new _d({ownerDocument:this.ownerDocument,label:this.emptyLabel});
this.dropDown.addChild(_1b);
}
}else{
this._updateSelection();
}
this._isLoaded=false;
this._childrenLoaded=true;
if(!this._loadingStore){
this._setValueAttr(this.value,false);
}
},_refreshState:function(){
if(this._started){
this.validate(this.focused);
}
},startup:function(){
this.inherited(arguments);
this._refreshState();
},_setValueAttr:function(_1c){
this.inherited(arguments);
_3.set(this.valueNode,"value",this.get("value"));
this._refreshState();
},_setDisabledAttr:function(_1d){
this.inherited(arguments);
this._refreshState();
},_setRequiredAttr:function(_1e){
this._set("required",_1e);
this.focusNode.setAttribute("aria-required",_1e);
this._refreshState();
},_setOptionsAttr:function(_1f){
this._isLoaded=false;
this._set("options",_1f);
},_setDisplay:function(_20){
var lbl=_20||this.emptyLabel;
this.containerNode.innerHTML="<span role=\"option\" aria-selected=\"true\" class=\"dijitReset dijitInline "+this.baseClass.replace(/\s+|$/g,"Label ")+"\">"+lbl+"</span>";
},validate:function(_21){
var _22=this.disabled||this.isValid(_21);
this._set("state",_22?"":(this._hasBeenBlurred?"Error":"Incomplete"));
this.focusNode.setAttribute("aria-invalid",_22?"false":"true");
var _23=_22?"":this._missingMsg;
if(_23&&this.focused&&this._hasBeenBlurred){
_f.show(_23,this.domNode,this.tooltipPosition,!this.isLeftToRight());
}else{
_f.hide(this.domNode);
}
this._set("message",_23);
return _22;
},isValid:function(){
return (!this.required||this.value===0||!(/^\s*$/.test(this.value||"")));
},reset:function(){
this.inherited(arguments);
_f.hide(this.domNode);
this._refreshState();
},postMixInProperties:function(){
this.inherited(arguments);
this._missingMsg=_7.getLocalization("dijit.form","validate",this.lang).missingMessage;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onselectstart",_6.stop);
this.domNode.setAttribute("aria-expanded","false");
},_setStyleAttr:function(_24){
this.inherited(arguments);
_4.toggle(this.domNode,this.baseClass.replace(/\s+|$/g,"FixedWidth "),!!this.domNode.style.width);
},isLoaded:function(){
return this._isLoaded;
},loadDropDown:function(_25){
this._loadChildren(true);
this._isLoaded=true;
_25();
},closeDropDown:function(){
this.inherited(arguments);
if(this.dropDown&&this.dropDown.menuTableNode){
this.dropDown.menuTableNode.style.width="";
}
},destroy:function(_26){
if(this.dropDown&&!this.dropDown._destroyed){
this.dropDown.destroyRecursive(_26);
delete this.dropDown;
}
this.inherited(arguments);
},_onFocus:function(){
this.validate(true);
this.inherited(arguments);
},_onBlur:function(){
_f.hide(this.domNode);
this.inherited(arguments);
this.validate(false);
}});
_14._Menu=_11;
return _14;
});
