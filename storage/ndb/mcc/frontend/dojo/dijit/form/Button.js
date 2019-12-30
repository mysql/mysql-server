//>>built
require({cache:{"url:dijit/form/templates/Button.html":"<span class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><span class=\"dijitReset dijitInline dijitButtonNode\"\n\t\tdata-dojo-attach-event=\"ondijitclick:_onClick\" role=\"presentation\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdata-dojo-attach-point=\"titleNode,focusNode\"\n\t\t\trole=\"button\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\" data-dojo-attach-point=\"iconNode\"></span\n\t\t\t><span class=\"dijitReset dijitToggleButtonIconChar\">&#x25CF;</span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t\tdata-dojo-attach-point=\"containerNode\"\n\t\t\t></span\n\t\t></span\n\t></span\n\t><input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\"\n\t\ttabIndex=\"-1\" aria-hidden=\"true\" data-dojo-attach-point=\"valueNode\"\n/></span>\n"}});
define("dijit/form/Button",["require","dojo/_base/declare","dojo/dom-class","dojo/has","dojo/_base/kernel","dojo/_base/lang","dojo/ready","./_FormWidget","./_ButtonMixin","dojo/text!./templates/Button.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
if(_4("dijit-legacy-requires")){
_7(0,function(){
var _b=["dijit/form/DropDownButton","dijit/form/ComboButton","dijit/form/ToggleButton"];
_1(_b);
});
}
return _2("dijit.form.Button",[_8,_9],{showLabel:true,iconClass:"dijitNoIcon",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitButton",templateString:_a,_setValueAttr:"valueNode",_onClick:function(e){
var ok=this.inherited(arguments);
if(ok){
if(this.valueNode){
this.valueNode.click();
e.preventDefault();
e.stopPropagation();
}
}
return ok;
},_fillContent:function(_c){
if(_c&&(!this.params||!("label" in this.params))){
var _d=_6.trim(_c.innerHTML);
if(_d){
this.label=_d;
}
}
},_setShowLabelAttr:function(_e){
if(this.containerNode){
_3.toggle(this.containerNode,"dijitDisplayNone",!_e);
}
this._set("showLabel",_e);
},setLabel:function(_f){
_5.deprecated("dijit.form.Button.setLabel() is deprecated.  Use set('label', ...) instead.","","2.0");
this.set("label",_f);
},_setLabelAttr:function(_10){
this.inherited(arguments);
if(!this.showLabel&&!("title" in this.params)){
this.titleNode.title=_6.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
}});
});
