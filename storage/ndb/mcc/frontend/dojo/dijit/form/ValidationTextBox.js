//>>built
require({cache:{"url:dijit/form/templates/ValidationTextBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\" role=\"presentation\"\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class=\"dijitReset dijitInputInner\" data-dojo-attach-point='textbox,focusNode' autocomplete=\"off\"\n\t\t\t${!nameAttrSetting} type='${type}'\n\t/></div\n></div>\n"}});
define("dijit/form/ValidationTextBox",["dojo/_base/declare","dojo/i18n","./TextBox","../Tooltip","dojo/text!./templates/ValidationTextBox.html","dojo/i18n!./nls/validate"],function(_1,_2,_3,_4,_5){
return _1("dijit.form.ValidationTextBox",_3,{templateString:_5,baseClass:"dijitTextBox dijitValidationTextBox",required:false,promptMessage:"",invalidMessage:"$_unset_$",missingMessage:"$_unset_$",message:"",constraints:{},regExp:".*",regExpGen:function(){
return this.regExp;
},state:"",tooltipPosition:[],_setValueAttr:function(){
this.inherited(arguments);
this.validate(this.focused);
},validator:function(_6,_7){
return (new RegExp("^(?:"+this.regExpGen(_7)+")"+(this.required?"":"?")+"$")).test(_6)&&(!this.required||!this._isEmpty(_6))&&(this._isEmpty(_6)||this.parse(_6,_7)!==undefined);
},_isValidSubset:function(){
return this.textbox.value.search(this._partialre)==0;
},isValid:function(){
return this.validator(this.textbox.value,this.constraints);
},_isEmpty:function(_8){
return (this.trim?/^\s*$/:/^$/).test(_8);
},getErrorMessage:function(){
return (this.required&&this._isEmpty(this.textbox.value))?this.missingMessage:this.invalidMessage;
},getPromptMessage:function(){
return this.promptMessage;
},_maskValidSubsetError:true,validate:function(_9){
var _a="";
var _b=this.disabled||this.isValid(_9);
if(_b){
this._maskValidSubsetError=true;
}
var _c=this._isEmpty(this.textbox.value);
var _d=!_b&&_9&&this._isValidSubset();
this._set("state",_b?"":(((((!this._hasBeenBlurred||_9)&&_c)||_d)&&this._maskValidSubsetError)?"Incomplete":"Error"));
this.focusNode.setAttribute("aria-invalid",_b?"false":"true");
if(this.state=="Error"){
this._maskValidSubsetError=_9&&_d;
_a=this.getErrorMessage(_9);
}else{
if(this.state=="Incomplete"){
_a=this.getPromptMessage(_9);
this._maskValidSubsetError=!this._hasBeenBlurred||_9;
}else{
if(_c){
_a=this.getPromptMessage(_9);
}
}
}
this.set("message",_a);
return _b;
},displayMessage:function(_e){
if(_e&&this.focused){
_4.show(_e,this.domNode,this.tooltipPosition,!this.isLeftToRight());
}else{
_4.hide(this.domNode);
}
},_refreshState:function(){
this.validate(this.focused);
this.inherited(arguments);
},constructor:function(){
this.constraints={};
},_setConstraintsAttr:function(_f){
if(!_f.locale&&this.lang){
_f.locale=this.lang;
}
this._set("constraints",_f);
this._computePartialRE();
},_computePartialRE:function(){
var p=this.regExpGen(this.constraints);
this.regExp=p;
var _10="";
if(p!=".*"){
this.regExp.replace(/\\.|\[\]|\[.*?[^\\]{1}\]|\{.*?\}|\(\?[=:!]|./g,function(re){
switch(re.charAt(0)){
case "{":
case "+":
case "?":
case "*":
case "^":
case "$":
case "|":
case "(":
_10+=re;
break;
case ")":
_10+="|$)";
break;
default:
_10+="(?:"+re+"|$)";
break;
}
});
}
try{
"".search(_10);
}
catch(e){
_10=this.regExp;
console.warn("RegExp error in "+this.declaredClass+": "+this.regExp);
}
this._partialre="^(?:"+_10+")$";
},postMixInProperties:function(){
this.inherited(arguments);
this.messages=_2.getLocalization("dijit.form","validate",this.lang);
if(this.invalidMessage=="$_unset_$"){
this.invalidMessage=this.messages.invalidMessage;
}
if(!this.invalidMessage){
this.invalidMessage=this.promptMessage;
}
if(this.missingMessage=="$_unset_$"){
this.missingMessage=this.messages.missingMessage;
}
if(!this.missingMessage){
this.missingMessage=this.invalidMessage;
}
this._setConstraintsAttr(this.constraints);
},_setDisabledAttr:function(_11){
this.inherited(arguments);
this._refreshState();
},_setRequiredAttr:function(_12){
this._set("required",_12);
this.focusNode.setAttribute("aria-required",_12);
this._refreshState();
},_setMessageAttr:function(_13){
this._set("message",_13);
this.displayMessage(_13);
},reset:function(){
this._maskValidSubsetError=true;
this.inherited(arguments);
},_onBlur:function(){
this.displayMessage("");
this.inherited(arguments);
}});
});
