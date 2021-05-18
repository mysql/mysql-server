//>>built
require({cache:{"url:dijit/form/templates/DropDownBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\"\n\trole=\"combobox\"\n\taria-haspopup=\"true\"\n\tdata-dojo-attach-point=\"_popupStateNode\"\n\t><div class='dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton dijitArrowButtonContainer'\n\t\tdata-dojo-attach-point=\"_buttonNode\" role=\"presentation\"\n\t\t><input class=\"dijitReset dijitInputField dijitArrowButtonInner\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"button presentation\" aria-hidden=\"true\"\n\t\t\t${_buttonInputDisabled}\n\t/></div\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class='dijitReset dijitInputInner' ${!nameAttrSetting} type=\"${type}\" autocomplete=\"off\"\n\t\t\tdata-dojo-attach-point=\"textbox,focusNode\" role=\"textbox\"\n\t/></div\n></div>\n"}});
define("dijit/form/_DateTimeTextBox",["dojo/date","dojo/date/locale","dojo/date/stamp","dojo/_base/declare","dojo/_base/lang","./RangeBoundTextBox","../_HasDropDown","dojo/text!./templates/DropDownBox.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
new Date("X");
var _9=_4("dijit.form._DateTimeTextBox",[_6,_7],{templateString:_8,hasDownArrow:true,cssStateNodes:{"_buttonNode":"dijitDownArrowButton"},_unboundedConstraints:{},pattern:_2.regexp,datePackage:"",postMixInProperties:function(){
this.inherited(arguments);
this._set("type","text");
},compare:function(_a,_b){
var _c=this._isInvalidDate(_a);
var _d=this._isInvalidDate(_b);
if(_c||_d){
return (_c&&_d)?0:(!_c?1:-1);
}
var _e=this.format(_a,this._unboundedConstraints),_f=this.format(_b,this._unboundedConstraints),_10=this.parse(_e,this._unboundedConstraints),_11=this.parse(_f,this._unboundedConstraints);
return _e==_f?0:_1.compare(_10,_11,this._selector);
},autoWidth:true,format:function(_12,_13){
if(!_12){
return "";
}
return this.dateLocaleModule.format(_12,_13);
},"parse":function(_14,_15){
return this.dateLocaleModule.parse(_14,_15)||(this._isEmpty(_14)?null:undefined);
},serialize:function(val,_16){
if(val.toGregorian){
val=val.toGregorian();
}
return _3.toISOString(val,_16);
},dropDownDefaultValue:new Date(),value:new Date(""),_blankValue:null,popupClass:"",_selector:"",constructor:function(_17){
_17=_17||{};
this.dateModule=_17.datePackage?_5.getObject(_17.datePackage,false):_1;
this.dateClassObj=this.dateModule.Date||Date;
if(!(this.dateClassObj instanceof Date)){
this.value=new this.dateClassObj(this.value);
}
this.dateLocaleModule=_17.datePackage?_5.getObject(_17.datePackage+".locale",false):_2;
this._set("pattern",this.dateLocaleModule.regexp);
this._invalidDate=this.constructor.prototype.value.toString();
},buildRendering:function(){
this.inherited(arguments);
if(!this.hasDownArrow){
this._buttonNode.style.display="none";
}
if(!this.hasDownArrow){
this._buttonNode=this.domNode;
this.baseClass+=" dijitComboBoxOpenOnClick";
}
},_setConstraintsAttr:function(_18){
_18.selector=this._selector;
_18.fullYear=true;
var _19=_3.fromISOString;
if(typeof _18.min=="string"){
_18.min=_19(_18.min);
if(!(this.dateClassObj instanceof Date)){
_18.min=new this.dateClassObj(_18.min);
}
}
if(typeof _18.max=="string"){
_18.max=_19(_18.max);
if(!(this.dateClassObj instanceof Date)){
_18.max=new this.dateClassObj(_18.max);
}
}
this.inherited(arguments);
this._unboundedConstraints=_5.mixin({},this.constraints,{min:null,max:null});
},_isInvalidDate:function(_1a){
return !_1a||isNaN(_1a)||typeof _1a!="object"||_1a.toString()==this._invalidDate;
},_setValueAttr:function(_1b,_1c,_1d){
if(_1b!==undefined){
if(typeof _1b=="string"){
_1b=_3.fromISOString(_1b);
}
if(this._isInvalidDate(_1b)){
_1b=null;
}
if(_1b instanceof Date&&!(this.dateClassObj instanceof Date)){
_1b=new this.dateClassObj(_1b);
}
}
this.inherited(arguments,[_1b,_1c,_1d]);
if(this.value instanceof Date){
this.filterString="";
}
if(_1c!==false&&this.dropDown){
this.dropDown.set("value",_1b,false);
}
},_set:function(_1e,_1f){
if(_1e=="value"){
if(_1f instanceof Date&&!(this.dateClassObj instanceof Date)){
_1f=new this.dateClassObj(_1f);
}
var _20=this._get("value");
if(_20 instanceof this.dateClassObj&&this.compare(_1f,_20)==0){
return;
}
}
this.inherited(arguments);
},_setDropDownDefaultValueAttr:function(val){
if(this._isInvalidDate(val)){
val=new this.dateClassObj();
}
this._set("dropDownDefaultValue",val);
},openDropDown:function(_21){
if(this.dropDown){
this.dropDown.destroy();
}
var _22=_5.isString(this.popupClass)?_5.getObject(this.popupClass,false):this.popupClass,_23=this,_24=this.get("value");
this.dropDown=new _22({onChange:function(_25){
_23.set("value",_25,true);
},id:this.id+"_popup",dir:_23.dir,lang:_23.lang,value:_24,textDir:_23.textDir,currentFocus:!this._isInvalidDate(_24)?_24:this.dropDownDefaultValue,constraints:_23.constraints,filterString:_23.filterString,datePackage:_23.datePackage,isDisabledDate:function(_26){
return !_23.rangeCheck(_26,_23.constraints);
}});
this.inherited(arguments);
},_getDisplayedValueAttr:function(){
return this.textbox.value;
},_setDisplayedValueAttr:function(_27,_28){
this._setValueAttr(this.parse(_27,this.constraints),_28,_27);
}});
return _9;
});
