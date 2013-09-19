//>>built
require({cache:{"url:dijit/form/templates/DropDownBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\"\n\trole=\"combobox\"\n\t><div class='dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton dijitArrowButtonContainer'\n\t\tdata-dojo-attach-point=\"_buttonNode, _popupStateNode\" role=\"presentation\"\n\t\t><input class=\"dijitReset dijitInputField dijitArrowButtonInner\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t${_buttonInputDisabled}\n\t/></div\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class='dijitReset dijitInputInner' ${!nameAttrSetting} type=\"text\" autocomplete=\"off\"\n\t\t\tdata-dojo-attach-point=\"textbox,focusNode\" role=\"textbox\" aria-haspopup=\"true\"\n\t/></div\n></div>\n"}});
define("dijit/form/_DateTimeTextBox",["dojo/date","dojo/date/locale","dojo/date/stamp","dojo/_base/declare","dojo/_base/lang","./RangeBoundTextBox","../_HasDropDown","dojo/text!./templates/DropDownBox.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
new Date("X");
var _9=_4("dijit.form._DateTimeTextBox",[_6,_7],{templateString:_8,hasDownArrow:true,openOnClick:true,regExpGen:_2.regexp,datePackage:_1,postMixInProperties:function(){
this.inherited(arguments);
this._set("type","text");
},compare:function(_a,_b){
var _c=this._isInvalidDate(_a);
var _d=this._isInvalidDate(_b);
return _c?(_d?0:-1):(_d?1:_1.compare(_a,_b,this._selector));
},forceWidth:true,format:function(_e,_f){
if(!_e){
return "";
}
return this.dateLocaleModule.format(_e,_f);
},"parse":function(_10,_11){
return this.dateLocaleModule.parse(_10,_11)||(this._isEmpty(_10)?null:undefined);
},serialize:function(val,_12){
if(val.toGregorian){
val=val.toGregorian();
}
return _3.toISOString(val,_12);
},dropDownDefaultValue:new Date(),value:new Date(""),_blankValue:null,popupClass:"",_selector:"",constructor:function(_13){
this.datePackage=_13.datePackage||this.datePackage;
this.dateFuncObj=typeof this.datePackage=="string"?_5.getObject(this.datePackage,false):this.datePackage;
this.dateClassObj=this.dateFuncObj.Date||Date;
this.dateLocaleModule=_5.getObject("locale",false,this.dateFuncObj);
this.regExpGen=this.dateLocaleModule.regexp;
this._invalidDate=this.constructor.prototype.value.toString();
},buildRendering:function(){
this.inherited(arguments);
if(!this.hasDownArrow){
this._buttonNode.style.display="none";
}
if(this.openOnClick||!this.hasDownArrow){
this._buttonNode=this.domNode;
this.baseClass+=" dijitComboBoxOpenOnClick";
}
},_setConstraintsAttr:function(_14){
_14.selector=this._selector;
_14.fullYear=true;
var _15=_3.fromISOString;
if(typeof _14.min=="string"){
_14.min=_15(_14.min);
}
if(typeof _14.max=="string"){
_14.max=_15(_14.max);
}
this.inherited(arguments);
},_isInvalidDate:function(_16){
return !_16||isNaN(_16)||typeof _16!="object"||_16.toString()==this._invalidDate;
},_setValueAttr:function(_17,_18,_19){
if(_17!==undefined){
if(typeof _17=="string"){
_17=_3.fromISOString(_17);
}
if(this._isInvalidDate(_17)){
_17=null;
}
if(_17 instanceof Date&&!(this.dateClassObj instanceof Date)){
_17=new this.dateClassObj(_17);
}
}
this.inherited(arguments);
if(this.dropDown){
this.dropDown.set("value",_17,false);
}
},_set:function(_1a,_1b){
if(_1a=="value"&&this.value instanceof Date&&this.compare(_1b,this.value)==0){
return;
}
this.inherited(arguments);
},_setDropDownDefaultValueAttr:function(val){
if(this._isInvalidDate(val)){
val=new this.dateClassObj();
}
this.dropDownDefaultValue=val;
},openDropDown:function(_1c){
if(this.dropDown){
this.dropDown.destroy();
}
var _1d=_5.isString(this.popupClass)?_5.getObject(this.popupClass,false):this.popupClass,_1e=this,_1f=this.get("value");
this.dropDown=new _1d({onChange:function(_20){
_9.superclass._setValueAttr.call(_1e,_20,true);
},id:this.id+"_popup",dir:_1e.dir,lang:_1e.lang,value:_1f,currentFocus:!this._isInvalidDate(_1f)?_1f:this.dropDownDefaultValue,constraints:_1e.constraints,filterString:_1e.filterString,datePackage:_1e.datePackage,isDisabledDate:function(_21){
return !_1e.rangeCheck(_21,_1e.constraints);
}});
this.inherited(arguments);
},_getDisplayedValueAttr:function(){
return this.textbox.value;
},_setDisplayedValueAttr:function(_22,_23){
this._setValueAttr(this.parse(_22,this.constraints),_23,_22);
}});
return _9;
});
