//>>built
require({cache:{"url:dojox/form/resources/TriStateCheckBox.html":"<div class=\"dijit dijitReset dijitInline\" role=\"presentation\"\n\t><div class=\"dojoxTriStateCheckBoxInner\" dojoAttachPoint=\"stateLabelNode\"></div\n\t><input ${!nameAttrSetting} type=\"${type}\" role=\"${type}\" dojoAttachPoint=\"focusNode\"\n\tclass=\"dijitReset dojoxTriStateCheckBoxInput\" dojoAttachEvent=\"onclick:_onClick\"\n/></div>\n"}});
define("dojox/form/TriStateCheckBox",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/query","dojo/dom-attr","dojo/text!./resources/TriStateCheckBox.html","dijit/form/Button","dijit/form/_ToggleButtonMixin","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.form.TriStateCheckBox",[_9,_a],{templateString:_8,baseClass:"dojoxTriStateCheckBox",type:"checkbox",states:"",_stateLabels:null,stateValue:null,_currentState:0,_stateType:"False",readOnly:false,checked:"",_aria_attr:"aria-checked",constructor:function(){
this.states=[false,"mixed",true];
this.checked=false;
this._stateLabels={"False":"&#9633;","True":"&#8730;","Mixed":"&#9632;"};
this.stateValues={"False":false,"True":"on","Mixed":"mixed"};
},_fillContent:function(_b){
},postCreate:function(){
_7.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
this.inherited(arguments);
},startup:function(){
this.set("checked",this.params.checked||this.states[this._currentState]);
_7.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
this.inherited(arguments);
},_setIconClassAttr:null,_setCheckedAttr:function(_c,_d){
var _e=_3.indexOf(this.states,_c),_f=false;
if(_e>=0){
this._currentState=_e;
this._stateType=this._getStateType(_c);
_7.set(this.focusNode,"value",this.stateValues[this._stateType]);
_7.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
this.inherited(arguments);
}else{
console.warn("Invalid state!");
}
},setChecked:function(_10){
_1.deprecated("setChecked("+_10+") is deprecated. Use set('checked',"+_10+") instead.","","2.0");
this.set("checked",_10);
},_setStatesAttr:function(_11){
if(_4.isArray(_11)){
this._set("states",_11);
}else{
if(_4.isString(_11)){
var map={"true":true,"false":false,"mixed":"mixed"};
_11=_11.split(/\s*,\s*/);
for(var i=0;i<_11.length;i++){
_11[i]=map[_11[i]]!==undefined?map[_11[i]]:false;
}
this._set("states",_11);
}
}
},_setReadOnlyAttr:function(_12){
this._set("readOnly",_12);
_7.set(this.focusNode,"readOnly",_12);
},_setValueAttr:function(_13,_14){
if(typeof _13=="string"&&(_3.indexOf(this.states,_13)<0)){
if(_13==""){
_13="on";
}
this.stateValues["True"]=_13;
_13=true;
}
if(this._created){
this._currentState=_3.indexOf(this.states,_13);
this.set("checked",_13,_14);
_7.set(this.focusNode,"value",this.stateValues[this._stateType]);
}
},_setValuesAttr:function(_15){
this.stateValues["True"]=_15[0]?_15[0]:this.stateValues["True"];
this.stateValues["Mixed"]=_15[1]?_15[1]:this.stateValues["Mixed"];
},_getValueAttr:function(){
return this.stateValues[this._stateType];
},reset:function(){
this._hasBeenBlurred=false;
this.set("states",this.params.states||[false,"mixed",true]);
this.stateValues=this.params.stateValues||{"False":false,"True":"on","Mixed":"mixed"};
this.set("values",this.params.values||[]);
this.set("checked",this.params.checked||this.states[0]);
},_onFocus:function(){
if(this.id){
_6("label[for='"+this.id+"']").addClass("dijitFocusedLabel");
}
this.inherited(arguments);
},_onBlur:function(){
if(this.id){
_6("label[for='"+this.id+"']").removeClass("dijitFocusedLabel");
}
this.mouseFocus=false;
this.inherited(arguments);
},_onClick:function(e){
if(this.readOnly||this.disabled){
_5.stop(e);
return false;
}
this.click();
return this.onClick(e);
},click:function(){
if(this._currentState>=this.states.length-1){
this._currentState=0;
}else{
if(this._currentState==-1){
this.fixState();
}else{
this._currentState++;
}
}
var _16=this._currentState;
this.set("checked",this.states[this._currentState]);
this._currentState=_16;
_7.set(this.stateLabelNode,"innerHTML",this._stateLabels[this._stateType]);
},fixState:function(){
this._currentState=this.states.length-1;
},_getStateType:function(_17){
return _17?(_17=="mixed"?"Mixed":"True"):"False";
},_onMouseDown:function(){
this.mouseFocus=true;
}});
});
