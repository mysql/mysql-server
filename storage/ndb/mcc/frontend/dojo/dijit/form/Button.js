//>>built
require({cache:{"url:dijit/form/templates/Button.html":"<span class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><span class=\"dijitReset dijitInline dijitButtonNode\"\n\t\tdata-dojo-attach-event=\"ondijitclick:__onClick\" role=\"presentation\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdata-dojo-attach-point=\"titleNode,focusNode\"\n\t\t\trole=\"button\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\" data-dojo-attach-point=\"iconNode\"></span\n\t\t\t><span class=\"dijitReset dijitToggleButtonIconChar\">&#x25CF;</span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t\tdata-dojo-attach-point=\"containerNode\"\n\t\t\t></span\n\t\t></span\n\t></span\n\t><input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\"\n\t\tdata-dojo-attach-event=\"onclick:_onClick\"\n\t\ttabIndex=\"-1\" aria-hidden=\"true\" data-dojo-attach-point=\"valueNode\"\n/></span>\n"}});
define("dijit/form/Button",["require","dojo/_base/declare","dojo/dom-class","dojo/has","dojo/_base/kernel","dojo/_base/lang","dojo/ready","./_FormWidget","./_ButtonMixin","dojo/text!./templates/Button.html","../a11yclick"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
if(_4("dijit-legacy-requires")){
_7(0,function(){
var _b=["dijit/form/DropDownButton","dijit/form/ComboButton","dijit/form/ToggleButton"];
_1(_b);
});
}
var _c=_2("dijit.form.Button"+(_4("dojo-bidi")?"_NoBidi":""),[_8,_9],{showLabel:true,iconClass:"dijitNoIcon",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitButton",templateString:_a,_setValueAttr:"valueNode",_setNameAttr:function(_d){
if(this.valueNode){
this.valueNode.setAttribute("name",_d);
}
},postCreate:function(){
this.inherited(arguments);
this._setLabelFromContainer();
},_setLabelFromContainer:function(){
if(this.containerNode&&!this.label){
this.label=_6.trim(this.containerNode.innerHTML);
this.onLabelSet();
}
},_setShowLabelAttr:function(_e){
if(this.containerNode){
_3.toggle(this.containerNode,"dijitDisplayNone",!_e);
}
this._set("showLabel",_e);
},setLabel:function(_f){
_5.deprecated("dijit.form.Button.setLabel() is deprecated.  Use set('label', ...) instead.","","2.0");
this.set("label",_f);
},onLabelSet:function(){
this.inherited(arguments);
if(!this.showLabel&&!("title" in this.params)){
this.titleNode.title=_6.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
}});
if(_4("dojo-bidi")){
_c=_2("dijit.form.Button",_c,{onLabelSet:function(){
this.inherited(arguments);
if(this.titleNode.title){
this.applyTextDir(this.titleNode,this.titleNode.title);
}
},_setTextDirAttr:function(_10){
if(this._created&&this.textDir!=_10){
this._set("textDir",_10);
this._setLabelAttr(this.label);
}
}});
}
return _c;
});
