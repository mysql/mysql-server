//>>built
require({cache:{"url:dijit/form/templates/DropDownButton.html":"<span class=\"dijit dijitReset dijitInline\"\n\t><span class='dijitReset dijitInline dijitButtonNode'\n\t\tdata-dojo-attach-event=\"ondijitclick:__onClick\" data-dojo-attach-point=\"_buttonNode\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdata-dojo-attach-point=\"focusNode,titleNode,_arrowWrapperNode,_popupStateNode\"\n\t\t\trole=\"button\" aria-haspopup=\"true\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\"\n\t\t\t\tdata-dojo-attach-point=\"iconNode\"\n\t\t\t></span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tdata-dojo-attach-point=\"containerNode\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t></span\n\t\t\t><span class=\"dijitReset dijitInline dijitArrowButtonInner\"></span\n\t\t\t><span class=\"dijitReset dijitInline dijitArrowButtonChar\">&#9660;</span\n\t\t></span\n\t></span\n\t><input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\" tabIndex=\"-1\"\n\t\tdata-dojo-attach-event=\"onclick:_onClick\" data-dojo-attach-point=\"valueNode\" aria-hidden=\"true\"\n/></span>\n"}});
define("dijit/form/DropDownButton",["dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/query","../registry","../popup","./Button","../_Container","../_HasDropDown","dojo/text!./templates/DropDownButton.html","../a11yclick"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dijit.form.DropDownButton",[_7,_8,_9],{baseClass:"dijitDropDownButton",templateString:_a,_fillContent:function(){
var _b=this.srcNodeRef;
var _c=this.containerNode;
if(_b&&_c){
while(_b.hasChildNodes()){
var _d=_b.firstChild;
if(_d.hasAttribute&&(_d.hasAttribute("data-dojo-type")||_d.hasAttribute("dojoType")||_d.hasAttribute("data-"+_2._scopeName+"-type")||_d.hasAttribute(_2._scopeName+"Type"))){
this.dropDownContainer=this.ownerDocument.createElement("div");
this.dropDownContainer.appendChild(_d);
}else{
_c.appendChild(_d);
}
}
}
},startup:function(){
if(this._started){
return;
}
if(!this.dropDown&&this.dropDownContainer){
this.dropDown=_5.byNode(this.dropDownContainer.firstChild);
delete this.dropDownContainer;
}
if(this.dropDown){
_6.hide(this.dropDown);
}
this.inherited(arguments);
},isLoaded:function(){
var _e=this.dropDown;
return (!!_e&&(!_e.href||_e.isLoaded));
},loadDropDown:function(_f){
var _10=this.dropDown;
var _11=_10.on("load",_3.hitch(this,function(){
_11.remove();
_f();
}));
_10.refresh();
},isFocusable:function(){
return this.inherited(arguments)&&!this._mouseDown;
}});
});
