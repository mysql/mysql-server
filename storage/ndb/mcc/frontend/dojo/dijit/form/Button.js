//>>built
require({cache:{"url:dijit/form/templates/Button.html":"<span class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><span class=\"dijitReset dijitInline dijitButtonNode\"\n\t\tdata-dojo-attach-event=\"ondijitclick:_onClick\" role=\"presentation\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdata-dojo-attach-point=\"titleNode,focusNode\"\n\t\t\trole=\"button\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\" data-dojo-attach-point=\"iconNode\"></span\n\t\t\t><span class=\"dijitReset dijitToggleButtonIconChar\">&#x25CF;</span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t\tdata-dojo-attach-point=\"containerNode\"\n\t\t\t></span\n\t\t></span\n\t></span\n\t><input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\"\n\t\ttabIndex=\"-1\" role=\"presentation\" data-dojo-attach-point=\"valueNode\"\n/></span>\n"}});
define("dijit/form/Button",["require","dojo/_base/declare","dojo/dom-class","dojo/_base/kernel","dojo/_base/lang","dojo/ready","./_FormWidget","./_ButtonMixin","dojo/text!./templates/Button.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
if(!_4.isAsync){
_6(0,function(){
var _a=["dijit/form/DropDownButton","dijit/form/ComboButton","dijit/form/ToggleButton"];
_1(_a);
});
}
return _2("dijit.form.Button",[_7,_8],{showLabel:true,iconClass:"dijitNoIcon",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitButton",templateString:_9,_setValueAttr:"valueNode",_onClick:function(e){
var ok=this.inherited(arguments);
if(ok){
if(this.valueNode){
this.valueNode.click();
e.preventDefault();
}
}
return ok;
},_fillContent:function(_b){
if(_b&&(!this.params||!("label" in this.params))){
var _c=_5.trim(_b.innerHTML);
if(_c){
this.label=_c;
}
}
},_setShowLabelAttr:function(_d){
if(this.containerNode){
_3.toggle(this.containerNode,"dijitDisplayNone",!_d);
}
this._set("showLabel",_d);
},setLabel:function(_e){
_4.deprecated("dijit.form.Button.setLabel() is deprecated.  Use set('label', ...) instead.","","2.0");
this.set("label",_e);
},_setLabelAttr:function(_f){
this.inherited(arguments);
if(!this.showLabel&&!("title" in this.params)){
this.titleNode.title=_5.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
}});
});
