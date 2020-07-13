//>>built
define("dijit/form/NumberTextBox",["dojo/_base/declare","dojo/_base/lang","dojo/i18n","dojo/string","dojo/number","./RangeBoundTextBox"],function(_1,_2,_3,_4,_5,_6){
var _7=function(_8){
var _8=_8||{},_9=_3.getLocalization("dojo.cldr","number",_3.normalizeLocale(_8.locale)),_a=_8.pattern?_8.pattern:_9[(_8.type||"decimal")+"Format"];
var _b;
if(typeof _8.places=="number"){
_b=_8.places;
}else{
if(typeof _8.places==="string"&&_8.places.length>0){
_b=_8.places.replace(/.*,/,"");
}else{
_b=(_a.indexOf(".")!=-1?_a.split(".")[1].replace(/[^#0]/g,"").length:0);
}
}
return {sep:_9.decimal,places:_b};
};
var _c=_1("dijit.form.NumberTextBoxMixin",null,{pattern:function(_d){
return "("+(this.focused&&this.editOptions?this._regExpGenerator(_2.delegate(_d,this.editOptions))+"|":"")+this._regExpGenerator(_d)+")";
},value:NaN,editOptions:{pattern:"#.######"},_formatter:_5.format,_regExpGenerator:_5.regexp,_decimalInfo:_7(),postMixInProperties:function(){
this.inherited(arguments);
this._set("type","text");
},_setConstraintsAttr:function(_e){
var _f=typeof _e.places=="number"?_e.places:0;
if(_f){
_f++;
}
if(typeof _e.max!="number"){
_e.max=9*Math.pow(10,15-_f);
}
if(typeof _e.min!="number"){
_e.min=-9*Math.pow(10,15-_f);
}
this.inherited(arguments,[_e]);
if(this.focusNode&&this.focusNode.value&&!isNaN(this.value)){
this.set("value",this.value);
}
this._decimalInfo=_7(_e);
},_onFocus:function(by){
if(this.disabled||this.readOnly){
return;
}
var val=this.get("value");
if(typeof val=="number"&&!isNaN(val)){
var _10=this.format(val,this.constraints);
if(_10!==undefined){
this.textbox.value=_10;
if(by!=="mouse"){
this.textbox.select();
}
}
}
this.inherited(arguments);
},format:function(_11,_12){
var _13=String(_11);
if(typeof _11!="number"){
return _13;
}
if(isNaN(_11)){
return "";
}
if(!("rangeCheck" in this&&this.rangeCheck(_11,_12))&&_12.exponent!==false&&/\de[-+]?\d/i.test(_13)){
return _13;
}
if(this.editOptions&&this.focused){
_12=_2.mixin({},_12,this.editOptions);
}
return this._formatter(_11,_12);
},_parser:_5.parse,parse:function(_14,_15){
var _16=_2.mixin({},_15,(this.editOptions&&this.focused)?this.editOptions:{});
if(this.focused&&_16.places!=null){
var _17=_16.places;
var _18=typeof _17==="number"?_17:Number(_17.split(",").pop());
_16.places="0,"+_18;
}
var v=this._parser(_14,_16);
if(this.editOptions&&this.focused&&isNaN(v)){
v=this._parser(_14,_15);
}
return v;
},_getDisplayedValueAttr:function(){
var v=this.inherited(arguments);
return isNaN(v)?this.textbox.value:v;
},filter:function(_19){
if(_19==null||typeof _19=="string"&&_19==""){
return NaN;
}else{
if(typeof _19=="number"&&!isNaN(_19)&&_19!=0){
_19=_5.round(_19,this._decimalInfo.places);
}
}
return this.inherited(arguments,[_19]);
},serialize:function(_1a,_1b){
return (typeof _1a!="number"||isNaN(_1a))?"":this.inherited(arguments);
},_setBlurValue:function(){
var val=_2.hitch(_2.delegate(this,{focused:true}),"get")("value");
this._setValueAttr(val,true);
},_setValueAttr:function(_1c,_1d,_1e){
if(_1c!==undefined&&_1e===undefined){
_1e=String(_1c);
if(typeof _1c=="number"){
if(isNaN(_1c)){
_1e="";
}else{
if(("rangeCheck" in this&&this.rangeCheck(_1c,this.constraints))||this.constraints.exponent===false||!/\de[-+]?\d/i.test(_1e)){
_1e=undefined;
}
}
}else{
if(!_1c){
_1e="";
_1c=NaN;
}else{
_1c=undefined;
}
}
}
this.inherited(arguments,[_1c,_1d,_1e]);
},_getValueAttr:function(){
var v=this.inherited(arguments);
if(isNaN(v)&&this.textbox.value!==""){
if(this.constraints.exponent!==false&&/\de[-+]?\d/i.test(this.textbox.value)&&(new RegExp("^"+_5._realNumberRegexp(_2.delegate(this.constraints))+"$").test(this.textbox.value))){
var n=Number(this.textbox.value);
return isNaN(n)?undefined:n;
}else{
return undefined;
}
}else{
return v;
}
},isValid:function(_1f){
if(!this.focused||this._isEmpty(this.textbox.value)){
return this.inherited(arguments);
}else{
var v=this.get("value");
if(!isNaN(v)&&this.rangeCheck(v,this.constraints)){
if(this.constraints.exponent!==false&&/\de[-+]?\d/i.test(this.textbox.value)){
return true;
}else{
return this.inherited(arguments);
}
}else{
return false;
}
}
},_isValidSubset:function(){
var _20=(typeof this.constraints.min=="number"),_21=(typeof this.constraints.max=="number"),_22=this.get("value");
if(isNaN(_22)||(!_20&&!_21)){
return this.inherited(arguments);
}
var _23=_22|0,_24=_22<0,_25=this.textbox.value.indexOf(this._decimalInfo.sep)!=-1,_26=this.maxLength||20,_27=_26-this.textbox.value.length,_28=_25?this.textbox.value.split(this._decimalInfo.sep)[1].replace(/[^0-9]/g,""):"";
var _29=_25?_23+"."+_28:_23+"";
var _2a=_4.rep("9",_27),_2b=_22,_2c=_22;
if(_24){
_2b=Number(_29+_2a);
}else{
_2c=Number(_29+_2a);
}
return !((_20&&_2c<this.constraints.min)||(_21&&_2b>this.constraints.max));
}});
var _2d=_1("dijit.form.NumberTextBox",[_6,_c],{baseClass:"dijitTextBox dijitNumberTextBox"});
_2d.Mixin=_c;
return _2d;
});
