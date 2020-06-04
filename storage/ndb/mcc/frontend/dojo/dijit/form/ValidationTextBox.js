//>>built
require({cache:{"url:dijit/form/templates/ValidationTextBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\" role=\"presentation\"\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class=\"dijitReset dijitInputInner\" data-dojo-attach-point='textbox,focusNode' autocomplete=\"off\"\n\t\t\t${!nameAttrSetting} type='${type}'\n\t/></div\n></div>\n"}});
define("dijit/form/ValidationTextBox",["dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/i18n","./TextBox","../Tooltip","dojo/text!./templates/ValidationTextBox.html","dojo/i18n!./nls/validate"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1("dijit.form.ValidationTextBox",_5,{templateString:_7,required:false,promptMessage:"",invalidMessage:"$_unset_$",missingMessage:"$_unset_$",message:"",constraints:{},pattern:".*",regExp:"",regExpGen:function(){
},state:"",tooltipPosition:[],_deprecateRegExp:function(_9,_a){
if(_a!=_8.prototype[_9]){
_2.deprecated("ValidationTextBox id="+this.id+", set('"+_9+"', ...) is deprecated.  Use set('pattern', ...) instead.","","2.0");
this.set("pattern",_a);
}
},_setRegExpGenAttr:function(_b){
this._deprecateRegExp("regExpGen",_b);
this._set("regExpGen",this._computeRegexp);
},_setRegExpAttr:function(_c){
this._deprecateRegExp("regExp",_c);
},_setValueAttr:function(){
this.inherited(arguments);
this._refreshState();
},validator:function(_d,_e){
return (new RegExp("^(?:"+this._computeRegexp(_e)+")"+(this.required?"":"?")+"$")).test(_d)&&(!this.required||!this._isEmpty(_d))&&(this._isEmpty(_d)||this.parse(_d,_e)!==undefined);
},_isValidSubset:function(){
return this.textbox.value.search(this._partialre)==0;
},isValid:function(){
return this.validator(this.textbox.value,this.get("constraints"));
},_isEmpty:function(_f){
return (this.trim?/^\s*$/:/^$/).test(_f);
},getErrorMessage:function(){
var _10=this.invalidMessage=="$_unset_$"?this.messages.invalidMessage:!this.invalidMessage?this.promptMessage:this.invalidMessage;
var _11=this.missingMessage=="$_unset_$"?this.messages.missingMessage:!this.missingMessage?_10:this.missingMessage;
return (this.required&&this._isEmpty(this.textbox.value))?_11:_10;
},getPromptMessage:function(){
return this.promptMessage;
},_maskValidSubsetError:true,validate:function(_12){
var _13="";
var _14=this.disabled||this.isValid(_12);
if(_14){
this._maskValidSubsetError=true;
}
var _15=this._isEmpty(this.textbox.value);
var _16=!_14&&_12&&this._isValidSubset();
this._set("state",_14?"":(((((!this._hasBeenBlurred||_12)&&_15)||_16)&&(this._maskValidSubsetError||(_16&&!this._hasBeenBlurred&&_12)))?"Incomplete":"Error"));
this.focusNode.setAttribute("aria-invalid",this.state=="Error"?"true":"false");
if(this.state=="Error"){
this._maskValidSubsetError=_12&&_16;
_13=this.getErrorMessage(_12);
}else{
if(this.state=="Incomplete"){
_13=this.getPromptMessage(_12);
this._maskValidSubsetError=!this._hasBeenBlurred||_12;
}else{
if(_15){
_13=this.getPromptMessage(_12);
}
}
}
this.set("message",_13);
return _14;
},displayMessage:function(_17){
if(_17&&this.focused){
_6.show(_17,this.domNode,this.tooltipPosition,!this.isLeftToRight());
}else{
_6.hide(this.domNode);
}
},_refreshState:function(){
if(this._created){
this.validate(this.focused);
}
this.inherited(arguments);
},constructor:function(_18){
this.constraints=_3.clone(this.constraints);
this.baseClass+=" dijitValidationTextBox";
},startup:function(){
this.inherited(arguments);
this._refreshState();
},_setConstraintsAttr:function(_19){
if(!_19.locale&&this.lang){
_19.locale=this.lang;
}
this._set("constraints",_19);
this._refreshState();
},_setPatternAttr:function(_1a){
this._set("pattern",_1a);
this._refreshState();
},_computeRegexp:function(_1b){
var p=this.pattern;
if(typeof p=="function"){
p=p.call(this,_1b);
}
if(p!=this._lastRegExp){
var _1c="";
this._lastRegExp=p;
if(p!=".*"){
p.replace(/\\.|\[\]|\[.*?[^\\]{1}\]|\{.*?\}|\(\?[=:!]|./g,function(re){
switch(re.charAt(0)){
case "{":
case "+":
case "?":
case "*":
case "^":
case "$":
case "|":
case "(":
_1c+=re;
break;
case ")":
_1c+="|$)";
break;
default:
_1c+="(?:"+re+"|$)";
break;
}
});
}
try{
"".search(_1c);
}
catch(e){
_1c=this.pattern;
console.warn("RegExp error in "+this.declaredClass+": "+this.pattern);
}
this._partialre="^(?:"+_1c+")$";
}
return p;
},postMixInProperties:function(){
this.inherited(arguments);
this.messages=_4.getLocalization("dijit.form","validate",this.lang);
this._setConstraintsAttr(this.constraints);
},_setDisabledAttr:function(_1d){
this.inherited(arguments);
this._refreshState();
},_setRequiredAttr:function(_1e){
this._set("required",_1e);
this.focusNode.setAttribute("aria-required",_1e);
this._refreshState();
},_setMessageAttr:function(_1f){
this._set("message",_1f);
this.displayMessage(_1f);
},reset:function(){
this._maskValidSubsetError=true;
this.inherited(arguments);
},_onBlur:function(){
this.displayMessage("");
this.inherited(arguments);
},destroy:function(){
_6.hide(this.domNode);
this.inherited(arguments);
}});
return _8;
});
