//>>built
require({cache:{"url:dijit/form/templates/CheckBox.html":"<div class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><input\n\t \t${!nameAttrSetting} type=\"${type}\" ${checkedAttrSetting}\n\t\tclass=\"dijitReset dijitCheckBoxInput\"\n\t\tdata-dojo-attach-point=\"focusNode\"\n\t \tdata-dojo-attach-event=\"onclick:_onClick\"\n/></div>\n"}});
define("dijit/form/CheckBox",["require","dojo/_base/declare","dojo/dom-attr","dojo/_base/kernel","dojo/query","dojo/ready","./ToggleButton","./_CheckBoxMixin","dojo/text!./templates/CheckBox.html","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
if(!_4.isAsync){
_6(0,function(){
var _a=["dijit/form/RadioButton"];
_1(_a);
});
}
return _2("dijit.form.CheckBox",[_7,_8],{templateString:_9,baseClass:"dijitCheckBox",_setValueAttr:function(_b,_c){
if(typeof _b=="string"){
this._set("value",_b);
_3.set(this.focusNode,"value",_b);
_b=true;
}
if(this._created){
this.set("checked",_b,_c);
}
},_getValueAttr:function(){
return (this.checked?this.value:false);
},_setIconClassAttr:null,postMixInProperties:function(){
this.inherited(arguments);
this.checkedAttrSetting=this.checked?"checked":"";
},_fillContent:function(){
},_onFocus:function(){
if(this.id){
_5("label[for='"+this.id+"']").addClass("dijitFocusedLabel");
}
this.inherited(arguments);
},_onBlur:function(){
if(this.id){
_5("label[for='"+this.id+"']").removeClass("dijitFocusedLabel");
}
this.inherited(arguments);
}});
});
