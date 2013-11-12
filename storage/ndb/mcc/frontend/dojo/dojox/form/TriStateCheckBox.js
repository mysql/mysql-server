//>>built
require({cache:{"url:dojox/form/resources/TriStateCheckBox.html":"<div class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><div class=\"dojoxTriStateCheckBoxInner\" dojoAttachPoint=\"stateLabelNode\"></div\n\t><input ${!nameAttrSetting} type=\"${type}\" dojoAttachPoint=\"focusNode\"\n\tclass=\"dijitReset dojoxTriStateCheckBoxInput\" dojoAttachEvent=\"onclick:_onClick\"\n/></div>"}});
define("dojox/form/TriStateCheckBox",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/query","dojo/dom-attr","dojo/text!./resources/TriStateCheckBox.html","dijit/form/ToggleButton"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.form.TriStateCheckBox",_8,{templateString:_7,baseClass:"dojoxTriStateCheckBox",type:"checkbox",_currentState:0,_stateType:"False",readOnly:false,constructor:function(){
this.states=[false,true,"mixed"];
this._stateLabels={"False":"&#63219","True":"&#8730;","Mixed":"&#8801"};
this.stateValues={"False":"off","True":"on","Mixed":"mixed"};
},_setIconClassAttr:null,_setCheckedAttr:function(_9,_a){
this._set("checked",_9);
this._currentState=_3.indexOf(this.states,_9);
this._stateType=this._getStateType(_9);
_6.set(this.focusNode||this.domNode,"checked",_9);
_6.set(this.focusNode,"value",this.stateValues[this._stateType]);
(this.focusNode||this.domNode).setAttribute("aria-checked",_9);
this._handleOnChange(_9,_a);
},setChecked:function(_b){
_1.deprecated("setChecked("+_b+") is deprecated. Use set('checked',"+_b+") instead.","","2.0");
this.set("checked",_b);
},_setReadOnlyAttr:function(_c){
this._set("readOnly",_c);
_6.set(this.focusNode,"readOnly",_c);
this.focusNode.setAttribute("aria-readonly",_c);
},_setValueAttr:function(_d,_e){
if(typeof _d=="string"&&(_3.indexOf(this.states,_d)<0)){
if(_d==""){
_d="on";
}
this.stateValues["True"]=_d;
_d=true;
}
if(this._created){
this._currentState=_3.indexOf(this.states,_d);
this.set("checked",_d,_e);
_6.set(this.focusNode,"value",this.stateValues[this._stateType]);
}
},_setValuesAttr:function(_f){
this.stateValues["True"]=_f[0]?_f[0]:this.stateValues["True"];
this.stateValues["Mixed"]=_f[1]?_f[1]:this.stateValues["False"];
},_getValueAttr:function(){
return this.stateValues[this._stateType];
},startup:function(){
this.set("checked",this.params.checked||this.states[this._currentState]);
_6.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
this.inherited(arguments);
},_fillContent:function(_10){
},reset:function(){
this._hasBeenBlurred=false;
this.stateValues={"False":"off","True":"on","Mixed":"mixed"};
this.set("checked",this.params.checked||this.states[0]);
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
},_onClick:function(e){
if(this.readOnly||this.disabled){
_4.stop(e);
return false;
}
if(this._currentState>=this.states.length-1){
this._currentState=0;
}else{
this._currentState++;
}
this.set("checked",this.states[this._currentState]);
_6.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
return this.onClick(e);
},_getStateType:function(_11){
return _11?(_11=="mixed"?"Mixed":"True"):"False";
}});
});
