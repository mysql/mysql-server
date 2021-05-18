//>>built
require({cache:{"url:dijit/form/templates/Select.html":"<table class=\"dijit dijitReset dijitInline dijitLeft\"\n\tdata-dojo-attach-point=\"_buttonNode,tableNode,focusNode,_popupStateNode\" cellspacing='0' cellpadding='0'\n\trole=\"listbox\" aria-haspopup=\"true\"\n\t><tbody role=\"presentation\"><tr role=\"presentation\"\n\t\t><td class=\"dijitReset dijitStretch dijitButtonContents\" role=\"presentation\"\n\t\t\t><div class=\"dijitReset dijitInputField dijitButtonText\"  data-dojo-attach-point=\"containerNode,textDirNode\" role=\"presentation\"></div\n\t\t\t><div class=\"dijitReset dijitValidationContainer\"\n\t\t\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t/></div\n\t\t\t><input type=\"hidden\" ${!nameAttrSetting} data-dojo-attach-point=\"valueNode\" value=\"${value}\" aria-hidden=\"true\"\n\t\t/></td\n\t\t><td class=\"dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton dijitArrowButtonContainer\"\n\t\t\tdata-dojo-attach-point=\"titleNode\" role=\"presentation\"\n\t\t\t><input class=\"dijitReset dijitInputField dijitArrowButtonInner\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t\t${_buttonInputDisabled}\n\t\t/></td\n\t></tr></tbody\n></table>\n"}});
define("dijit/form/Select",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","./_FormSelectWidget","../_HasDropDown","../DropDownMenu","../MenuItem","../MenuSeparator","../Tooltip","../_KeyNavMixin","../registry","dojo/text!./templates/Select.html","dojo/i18n!./nls/validate"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
var _13=_2("dijit.form._SelectMenu",_c,{autoFocus:true,buildRendering:function(){
this.inherited(arguments);
this.domNode.setAttribute("role","listbox");
},postCreate:function(){
this.inherited(arguments);
this.own(on(this.domNode,"selectstart",function(evt){
evt.preventDefault();
evt.stopPropagation();
}));
},focus:function(){
var _14=false,val=this.parentWidget.value;
if(_8.isArray(val)){
val=val[val.length-1];
}
if(val){
_1.forEach(this.parentWidget._getChildren(),function(_15){
if(_15.option&&(val===_15.option.value)){
_14=true;
this.focusChild(_15,false);
}
},this);
}
if(!_14){
this.inherited(arguments);
}
}});
var _16=_2("dijit.form.Select"+(_9("dojo-bidi")?"_NoBidi":""),[_a,_b,_10],{baseClass:"dijitSelect dijitValidationTextBox",templateString:_12,_buttonInputDisabled:_9("ie")?"disabled":"",required:false,state:"",message:"",tooltipPosition:[],emptyLabel:"&#160;",_isLoaded:false,_childrenLoaded:false,labelType:"html",_fillContent:function(){
this.inherited(arguments);
if(this.options.length&&!this.value&&this.srcNodeRef){
var si=this.srcNodeRef.selectedIndex||0;
this._set("value",this.options[si>=0?si:0].value);
}
this.dropDown=new _13({id:this.id+"_menu",parentWidget:this});
_4.add(this.dropDown.domNode,this.baseClass.replace(/\s+|$/g,"Menu "));
},_getMenuItemForOption:function(_17){
if(!_17.value&&!_17.label){
return new _e({ownerDocument:this.ownerDocument});
}else{
var _18=_8.hitch(this,"_setValueAttr",_17);
var _19=new _d({option:_17,label:(this.labelType==="text"?(_17.label||"").toString().replace(/&/g,"&amp;").replace(/</g,"&lt;"):_17.label)||this.emptyLabel,onClick:_18,ownerDocument:this.ownerDocument,dir:this.dir,textDir:this.textDir,disabled:_17.disabled||false});
_19.focusNode.setAttribute("role","option");
return _19;
}
},_addOptionItem:function(_1a){
if(this.dropDown){
this.dropDown.addChild(this._getMenuItemForOption(_1a));
}
},_getChildren:function(){
if(!this.dropDown){
return [];
}
return this.dropDown.getChildren();
},focus:function(){
if(!this.disabled&&this.focusNode.focus){
try{
this.focusNode.focus();
}
catch(e){
}
}
},focusChild:function(_1b){
if(_1b){
this.set("value",_1b.option);
}
},_getFirst:function(){
var _1c=this._getChildren();
return _1c.length?_1c[0]:null;
},_getLast:function(){
var _1d=this._getChildren();
return _1d.length?_1d[_1d.length-1]:null;
},childSelector:function(_1e){
var _1e=_11.byNode(_1e);
return _1e&&_1e.getParent()==this.dropDown;
},onKeyboardSearch:function(_1f,evt,_20,_21){
if(_1f){
this.focusChild(_1f);
}
},_loadChildren:function(_22){
if(_22===true){
if(this.dropDown){
delete this.dropDown.focusedChild;
this.focusedChild=null;
}
if(this.options.length){
this.inherited(arguments);
}else{
_1.forEach(this._getChildren(),function(_23){
_23.destroyRecursive();
});
var _24=new _d({ownerDocument:this.ownerDocument,label:this.emptyLabel});
this.dropDown.addChild(_24);
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
},_setValueAttr:function(_25){
this.inherited(arguments);
_3.set(this.valueNode,"value",this.get("value"));
this._refreshState();
},_setNameAttr:"valueNode",_setDisabledAttr:function(_26){
this.inherited(arguments);
this._refreshState();
},_setRequiredAttr:function(_27){
this._set("required",_27);
this.focusNode.setAttribute("aria-required",_27);
this._refreshState();
},_setOptionsAttr:function(_28){
this._isLoaded=false;
this._set("options",_28);
},_setDisplay:function(_29){
var lbl=(this.labelType==="text"?(_29||"").replace(/&/g,"&amp;").replace(/</g,"&lt;"):_29)||this.emptyLabel;
this.containerNode.innerHTML="<span role=\"option\" aria-selected=\"true\" class=\"dijitReset dijitInline "+this.baseClass.replace(/\s+|$/g,"Label ")+"\">"+lbl+"</span>";
},validate:function(_2a){
var _2b=this.disabled||this.isValid(_2a);
this._set("state",_2b?"":(this._hasBeenBlurred?"Error":"Incomplete"));
this.focusNode.setAttribute("aria-invalid",_2b?"false":"true");
var _2c=_2b?"":this._missingMsg;
if(_2c&&this.focused&&this._hasBeenBlurred){
_f.show(_2c,this.domNode,this.tooltipPosition,!this.isLeftToRight());
}else{
_f.hide(this.domNode);
}
this._set("message",_2c);
return _2b;
},isValid:function(){
return (!this.required||this.value===0||!(/^\s*$/.test(this.value||"")));
},reset:function(){
this.inherited(arguments);
_f.hide(this.domNode);
this._refreshState();
},postMixInProperties:function(){
this.inherited(arguments);
this._missingMsg=_6.getLocalization("dijit.form","validate",this.lang).missingMessage;
},postCreate:function(){
this.inherited(arguments);
this.own(on(this.domNode,"selectstart",function(evt){
evt.preventDefault();
evt.stopPropagation();
}));
this.domNode.setAttribute("aria-expanded","false");
var _2d=this._keyNavCodes;
delete _2d[_7.LEFT_ARROW];
delete _2d[_7.RIGHT_ARROW];
},_setStyleAttr:function(_2e){
this.inherited(arguments);
_4.toggle(this.domNode,this.baseClass.replace(/\s+|$/g,"FixedWidth "),!!this.domNode.style.width);
},isLoaded:function(){
return this._isLoaded;
},loadDropDown:function(_2f){
this._loadChildren(true);
this._isLoaded=true;
_2f();
},destroy:function(_30){
if(this.dropDown&&!this.dropDown._destroyed){
this.dropDown.destroyRecursive(_30);
delete this.dropDown;
}
_f.hide(this.domNode);
this.inherited(arguments);
},_onFocus:function(){
this.validate(true);
},_onBlur:function(){
_f.hide(this.domNode);
this.inherited(arguments);
this.validate(false);
}});
if(_9("dojo-bidi")){
_16=_2("dijit.form.Select",_16,{_setDisplay:function(_31){
this.inherited(arguments);
this.applyTextDir(this.containerNode);
}});
}
_16._Menu=_13;
function _32(_33){
return function(evt){
if(!this._isLoaded){
this.loadDropDown(_8.hitch(this,_33,evt));
}else{
this.inherited(_33,arguments);
}
};
};
_16.prototype._onContainerKeydown=_32("_onContainerKeydown");
_16.prototype._onContainerKeypress=_32("_onContainerKeypress");
return _16;
});
