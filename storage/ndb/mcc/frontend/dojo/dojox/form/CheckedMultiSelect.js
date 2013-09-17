//>>built
require({cache:{"url:dojox/form/resources/_CheckedMultiSelectMenuItem.html":"<tr class=\"dijitReset dijitMenuItem\" dojoAttachPoint=\"focusNode\" role=\"menuitemcheckbox\" tabIndex=\"-1\"\n\tdojoAttachEvent=\"onmouseenter:_onHover,onmouseleave:_onUnhover,ondijitclick:_onClick\"\n\t><td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\"\n\t\t><div src=\"${_blankGif}\" alt=\"\" class=\"dijitMenuItemIcon ${_iconClass}\" dojoAttachPoint=\"iconNode\"\n\t\t\t><input class=\"dojoxCheckedMultiSelectCheckBoxInput\" dojoAttachPoint=\"inputNode\" type=\"${_type.type}\"\n\t\t/></div></td\n\t><td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" dojoAttachPoint=\"containerNode,labelNode\"></td\n\t><td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" dojoAttachPoint=\"accelKeyNode\"></td\n\t><td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">&nbsp;</td\n></tr>","url:dojox/form/resources/_CheckedMultiSelectItem.html":"<div class=\"dijitReset ${baseClass}\"\n\t><input class=\"${baseClass}Box\" data-dojo-type=\"dijit.form.CheckBox\" data-dojo-attach-point=\"checkBox\" \n\t\tdata-dojo-attach-event=\"_onClick:_changeBox\" type=\"${_type.type}\" baseClass=\"${_type.baseClass}\"\n\t/><div class=\"dijitInline ${baseClass}Label\" data-dojo-attach-point=\"labelNode\" data-dojo-attach-event=\"onclick:_onClick\"></div\n></div>\n","url:dojox/form/resources/CheckedMultiSelect.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\" id=\"widget_${id}\"\n\t><div data-dojo-attach-point=\"comboButtonNode\"\n\t></div\n\t><div data-dojo-attach-point=\"selectNode\" class=\"dijit dijitReset dijitInline ${baseClass}Wrapper\" data-dojo-attach-event=\"onmousedown:_onMouseDown,onclick:focus\"\n\t\t><select class=\"${baseClass}Select dojoxCheckedMultiSelectHidden\" multiple=\"true\" data-dojo-attach-point=\"containerNode,focusNode\"></select\n\t\t><div data-dojo-attach-point=\"wrapperDiv\"></div\n\t></div\n></div>"}});
define("dojox/form/CheckedMultiSelect",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/event","dojo/dom-geometry","dojo/dom-class","dojo/dom-construct","dojo/i18n","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/registry","dijit/Menu","dijit/MenuItem","dijit/Tooltip","dijit/form/_FormSelectWidget","dijit/form/ComboButton","dojo/text!dojox/form/resources/_CheckedMultiSelectMenuItem.html","dojo/text!dojox/form/resources/_CheckedMultiSelectItem.html","dojo/text!dojox/form/resources/CheckedMultiSelect.html","dojo/i18n!dojox/form/nls/CheckedMultiSelect","dijit/form/CheckBox"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15){
var _16=_1("dojox.form._CheckedMultiSelectItem",[_9,_a,_b],{templateString:_13,baseClass:"dojoxMultiSelectItem",option:null,parent:null,disabled:false,readOnly:false,postMixInProperties:function(){
this._type=this.parent.multiple?{type:"checkbox",baseClass:"dijitCheckBox"}:{type:"radio",baseClass:"dijitRadio"};
this.disabled=this.option.disabled=this.option.disabled||false;
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.labelNode.innerHTML=this.option.label;
},_changeBox:function(){
if(this.get("disabled")||this.get("readOnly")){
return;
}
if(this.parent.multiple){
this.option.selected=this.checkBox.get("value")&&true;
}else{
this.parent.set("value",this.option.value);
}
this.parent._updateSelection();
this.parent.focus();
},_onClick:function(e){
if(this.get("disabled")||this.get("readOnly")){
_4.stop(e);
}else{
this.checkBox._onClick(e);
}
},_updateBox:function(){
this.checkBox.set("value",this.option.selected);
},_setDisabledAttr:function(_17){
this.disabled=_17||this.option.disabled;
this.checkBox.set("disabled",this.disabled);
_6.toggle(this.domNode,"dojoxMultiSelectDisabled",this.disabled);
},_setReadOnlyAttr:function(_18){
this.checkBox.set("readOnly",_18);
this.readOnly=_18;
}});
var _19=_1("dojox.form._CheckedMultiSelectMenu",_d,{multiple:false,buildRendering:function(){
this.inherited(arguments);
var o=(this.menuTableNode=this.domNode),n=(this.domNode=_7.create("div",{style:{overflowX:"hidden",overflowY:"scroll"}}));
if(o.parentNode){
o.parentNode.replaceChild(n,o);
}
_6.remove(o,"dijitMenuTable");
n.className=o.className+" dojoxCheckedMultiSelectMenu";
o.className="dijitReset dijitMenuTable";
o.setAttribute("role","listbox");
n.setAttribute("role","presentation");
n.appendChild(o);
},resize:function(mb){
if(mb){
_5.setMarginBox(this.domNode,mb);
if("w" in mb){
this.menuTableNode.style.width="100%";
}
}
},onClose:function(){
this.inherited(arguments);
if(this.menuTableNode){
this.menuTableNode.style.width="";
}
},onItemClick:function(_1a,evt){
if(typeof this.isShowingNow=="undefined"){
this._markActive();
}
this.focusChild(_1a);
if(_1a.disabled||_1a.readOnly){
return false;
}
if(!this.multiple){
this.onExecute();
}
_1a.onClick(evt);
}});
var _1b=_1("dojox.form._CheckedMultiSelectMenuItem",_e,{templateString:_12,option:null,parent:null,_iconClass:"",postMixInProperties:function(){
if(this.parent.multiple){
this._iconClass="dojoxCheckedMultiSelectMenuCheckBoxItemIcon";
this._type={type:"checkbox"};
}else{
this._iconClass="";
this._type={type:"hidden"};
}
this.disabled=this.option.disabled;
this.checked=this.option.selected;
this.label=this.option.label;
this.readOnly=this.option.readOnly;
this.inherited(arguments);
},onChange:function(_1c){
},_updateBox:function(){
_6.toggle(this.domNode,"dojoxCheckedMultiSelectMenuItemChecked",!!this.option.selected);
this.domNode.setAttribute("aria-checked",this.option.selected);
this.inputNode.checked=this.option.selected;
if(!this.parent.multiple){
_6.toggle(this.domNode,"dijitSelectSelectedOption",!!this.option.selected);
}
},_onClick:function(e){
if(!this.disabled&&!this.readOnly){
if(this.parent.multiple){
this.option.selected=!this.option.selected;
this.parent.onChange();
this.onChange(this.option.selected);
}else{
if(!this.option.selected){
_3.forEach(this.parent.getChildren(),function(_1d){
_1d.option.selected=false;
});
this.option.selected=true;
this.parent.onChange();
this.onChange(this.option.selected);
}
}
}
this.inherited(arguments);
}});
var _1e=_1("dojox.form.CheckedMultiSelect",_10,{templateString:_14,baseClass:"dojoxCheckedMultiSelect",required:false,invalidMessage:"$_unset_$",_message:"",dropDown:false,labelText:"",tooltipPosition:[],setStore:function(_1f,_20,_21){
this.inherited(arguments);
var _22=function(_23){
var _24=_3.map(_23,function(_25){
return _25.value[0];
});
if(_24.length){
this.set("value",_24);
}
};
this.store.fetch({query:{selected:true},onComplete:_22,scope:this});
},postMixInProperties:function(){
this.inherited(arguments);
this._nlsResources=_8.getLocalization("dojox.form","CheckedMultiSelect",this.lang);
if(this.invalidMessage=="$_unset_$"){
this.invalidMessage=this._nlsResources.invalidMessage;
}
},_fillContent:function(){
this.inherited(arguments);
if(this.options.length&&!this.value&&this.srcNodeRef){
var si=this.srcNodeRef.selectedIndex||0;
this.value=this.options[si>=0?si:0].value;
}
if(this.dropDown){
_6.toggle(this.selectNode,"dojoxCheckedMultiSelectHidden");
this.dropDownMenu=new _19({id:this.id+"_menu",style:"display: none;",multiple:this.multiple,onChange:_2.hitch(this,"_updateSelection")});
}
},startup:function(){
this.inherited(arguments);
if(this.dropDown){
this.dropDownButton=new _11({label:this.labelText,dropDown:this.dropDownMenu,baseClass:"dojoxCheckedMultiSelectButton",maxHeight:this.maxHeight},this.comboButtonNode);
}
},_onMouseDown:function(e){
_4.stop(e);
},validator:function(){
if(!this.required){
return true;
}
return _3.some(this.getOptions(),function(opt){
return opt.selected&&opt.value!=null&&opt.value.toString().length!=0;
});
},validate:function(_26){
_f.hide(this.domNode);
var _27=this.isValid(_26);
if(!_27){
this.displayMessage(this.invalidMessage);
}
return _27;
},isValid:function(_28){
return this.validator();
},getErrorMessage:function(_29){
return this.invalidMessage;
},displayMessage:function(_2a){
_f.hide(this.domNode);
if(_2a){
_f.show(_2a,this.domNode,this.tooltipPosition);
}
},onAfterAddOptionItem:function(_2b,_2c){
},_addOptionItem:function(_2d){
var _2e;
if(this.dropDown){
_2e=new _1b({option:_2d,parent:this.dropDownMenu});
this.dropDownMenu.addChild(_2e);
}else{
_2e=new _16({option:_2d,parent:this});
this.wrapperDiv.appendChild(_2e.domNode);
}
this.onAfterAddOptionItem(_2e,_2d);
},_refreshState:function(){
this.validate(this.focused);
},onChange:function(_2f){
this._refreshState();
},reset:function(){
this.inherited(arguments);
_f.hide(this.domNode);
},_updateSelection:function(){
this.inherited(arguments);
this._handleOnChange(this.value);
_3.forEach(this._getChildren(),function(_30){
_30._updateBox();
});
if(this.dropDown&&this.dropDownButton){
var i=0,_31="";
_3.forEach(this.options,function(_32){
if(_32.selected){
i++;
_31=_32.label;
}
});
this.dropDownButton.set("label",this.multiple?_2.replace(this._nlsResources.multiSelectLabelText,{num:i}):_31);
}
},_getChildren:function(){
if(this.dropDown){
return this.dropDownMenu.getChildren();
}else{
return _3.map(this.wrapperDiv.childNodes,function(n){
return _c.byNode(n);
});
}
},invertSelection:function(_33){
if(this.multiple){
_3.forEach(this.options,function(i){
i.selected=!i.selected;
});
this._updateSelection();
}
},_setDisabledAttr:function(_34){
this.inherited(arguments);
if(this.dropDown){
this.dropDownButton.set("disabled",_34);
}
_3.forEach(this._getChildren(),function(_35){
if(_35&&_35.set){
_35.set("disabled",_34);
}
});
},_setReadOnlyAttr:function(_36){
this.inherited(arguments);
if("readOnly" in this.attributeMap){
this._attrToDom("readOnly",_36);
}
this.readOnly=_36;
_3.forEach(this._getChildren(),function(_37){
if(_37&&_37.set){
_37.set("readOnly",_36);
}
});
},uninitialize:function(){
_f.hide(this.domNode);
_3.forEach(this._getChildren(),function(_38){
_38.destroyRecursive();
});
this.inherited(arguments);
}});
return _1e;
});
