//>>built
require({cache:{"url:dijit/form/templates/TextBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\" id=\"widget_${id}\" role=\"presentation\"\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class=\"dijitReset dijitInputInner\" data-dojo-attach-point='textbox,focusNode' autocomplete=\"off\"\n\t\t\t${!nameAttrSetting} type='${type}'\n\t/></div\n></div>\n"}});
define("dijit/form/TextBox",["dojo/_base/declare","dojo/dom-construct","dojo/dom-style","dojo/_base/kernel","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","./_FormValueWidget","./_TextBoxMixin","dojo/text!./templates/TextBox.html",".."],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_1([_8,_9],{templateString:_a,_singleNodeTemplate:"<input class=\"dijit dijitReset dijitLeft dijitInputField\" data-dojo-attach-point=\"textbox,focusNode\" autocomplete=\"off\" type=\"${type}\" ${!nameAttrSetting} />",_buttonInputDisabled:_6("ie")?"disabled":"",baseClass:"dijitTextBox",postMixInProperties:function(){
var _d=this.type.toLowerCase();
if(this.templateString&&this.templateString.toLowerCase()=="input"||((_d=="hidden"||_d=="file")&&this.templateString==this.constructor.prototype.templateString)){
this.templateString=this._singleNodeTemplate;
}
this.inherited(arguments);
},_onInput:function(e){
this.inherited(arguments);
if(this.intermediateChanges){
var _e=this;
setTimeout(function(){
_e._handleOnChange(_e.get("value"),false);
},0);
}
},_setPlaceHolderAttr:function(v){
this._set("placeHolder",v);
if(!this._phspan){
this._attachPoints.push("_phspan");
this._phspan=_2.create("span",{className:"dijitPlaceHolder dijitInputField"},this.textbox,"after");
}
this._phspan.innerHTML="";
this._phspan.appendChild(document.createTextNode(v));
this._updatePlaceHolder();
},_updatePlaceHolder:function(){
if(this._phspan){
this._phspan.style.display=(this.placeHolder&&!this.focused&&!this.textbox.value)?"":"none";
}
},_setValueAttr:function(_f,_10,_11){
this.inherited(arguments);
this._updatePlaceHolder();
},getDisplayedValue:function(){
_4.deprecated(this.declaredClass+"::getDisplayedValue() is deprecated. Use set('displayedValue') instead.","","2.0");
return this.get("displayedValue");
},setDisplayedValue:function(_12){
_4.deprecated(this.declaredClass+"::setDisplayedValue() is deprecated. Use set('displayedValue', ...) instead.","","2.0");
this.set("displayedValue",_12);
},_onBlur:function(e){
if(this.disabled){
return;
}
this.inherited(arguments);
this._updatePlaceHolder();
},_onFocus:function(by){
if(this.disabled||this.readOnly){
return;
}
this.inherited(arguments);
this._updatePlaceHolder();
}});
if(_6("ie")){
_c=_1(_c,{declaredClass:"dijit.form.TextBox",_isTextSelected:function(){
var _13=_7.doc.selection.createRange();
var _14=_13.parentElement();
return _14==this.textbox&&_13.text.length==0;
},postCreate:function(){
this.inherited(arguments);
setTimeout(_5.hitch(this,function(){
try{
var s=_3.getComputedStyle(this.domNode);
if(s){
var ff=s.fontFamily;
if(ff){
var _15=this.domNode.getElementsByTagName("INPUT");
if(_15){
for(var i=0;i<_15.length;i++){
_15[i].style.fontFamily=ff;
}
}
}
}
}
catch(e){
}
}),0);
}});
_b._setSelectionRange=_9._setSelectionRange=function(_16,_17,_18){
if(_16.createTextRange){
var r=_16.createTextRange();
r.collapse(true);
r.moveStart("character",-99999);
r.moveStart("character",_17);
r.moveEnd("character",_18-_17);
r.select();
}
};
}else{
if(_6("mozilla")){
_c=_1(_c,{declaredClass:"dijit.form.TextBox",_onBlur:function(e){
this.inherited(arguments);
if(this.selectOnClick){
this.textbox.selectionStart=this.textbox.selectionEnd=undefined;
}
}});
}else{
_c.prototype.declaredClass="dijit.form.TextBox";
}
}
_5.setObject("dijit.form.TextBox",_c);
return _c;
});
