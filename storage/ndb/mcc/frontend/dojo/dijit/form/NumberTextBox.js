//>>built
define("dijit/form/NumberTextBox",["dojo/_base/declare","dojo/_base/lang","dojo/number","./RangeBoundTextBox"],function(_1,_2,_3,_4){
var _5=_1("dijit.form.NumberTextBoxMixin",null,{regExpGen:_3.regexp,value:NaN,editOptions:{pattern:"#.######"},_formatter:_3.format,postMixInProperties:function(){
this.inherited(arguments);
this._set("type","text");
},_setConstraintsAttr:function(_6){
var _7=typeof _6.places=="number"?_6.places:0;
if(_7){
_7++;
}
if(typeof _6.max!="number"){
_6.max=9*Math.pow(10,15-_7);
}
if(typeof _6.min!="number"){
_6.min=-9*Math.pow(10,15-_7);
}
this.inherited(arguments,[_6]);
if(this.focusNode&&this.focusNode.value&&!isNaN(this.value)){
this.set("value",this.value);
}
},_onFocus:function(){
if(this.disabled){
return;
}
var _8=this.get("value");
if(typeof _8=="number"&&!isNaN(_8)){
var _9=this.format(_8,this.constraints);
if(_9!==undefined){
this.textbox.value=_9;
}
}
this.inherited(arguments);
},format:function(_a,_b){
var _c=String(_a);
if(typeof _a!="number"){
return _c;
}
if(isNaN(_a)){
return "";
}
if(!("rangeCheck" in this&&this.rangeCheck(_a,_b))&&_b.exponent!==false&&/\de[-+]?\d/i.test(_c)){
return _c;
}
if(this.editOptions&&this.focused){
_b=_2.mixin({},_b,this.editOptions);
}
return this._formatter(_a,_b);
},_parser:_3.parse,parse:function(_d,_e){
var v=this._parser(_d,_2.mixin({},_e,(this.editOptions&&this.focused)?this.editOptions:{}));
if(this.editOptions&&this.focused&&isNaN(v)){
v=this._parser(_d,_e);
}
return v;
},_getDisplayedValueAttr:function(){
var v=this.inherited(arguments);
return isNaN(v)?this.textbox.value:v;
},filter:function(_f){
return (_f===null||_f===""||_f===undefined)?NaN:this.inherited(arguments);
},serialize:function(_10,_11){
return (typeof _10!="number"||isNaN(_10))?"":this.inherited(arguments);
},_setBlurValue:function(){
var val=_2.hitch(_2.mixin({},this,{focused:true}),"get")("value");
this._setValueAttr(val,true);
},_setValueAttr:function(_12,_13,_14){
if(_12!==undefined&&_14===undefined){
_14=String(_12);
if(typeof _12=="number"){
if(isNaN(_12)){
_14="";
}else{
if(("rangeCheck" in this&&this.rangeCheck(_12,this.constraints))||this.constraints.exponent===false||!/\de[-+]?\d/i.test(_14)){
_14=undefined;
}
}
}else{
if(!_12){
_14="";
_12=NaN;
}else{
_12=undefined;
}
}
}
this.inherited(arguments,[_12,_13,_14]);
},_getValueAttr:function(){
var v=this.inherited(arguments);
if(isNaN(v)&&this.textbox.value!==""){
if(this.constraints.exponent!==false&&/\de[-+]?\d/i.test(this.textbox.value)&&(new RegExp("^"+_3._realNumberRegexp(_2.mixin({},this.constraints))+"$").test(this.textbox.value))){
var n=Number(this.textbox.value);
return isNaN(n)?undefined:n;
}else{
return undefined;
}
}else{
return v;
}
},isValid:function(_15){
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
}});
var _16=_1("dijit.form.NumberTextBox",[_4,_5],{baseClass:"dijitTextBox dijitNumberTextBox"});
_16.Mixin=_5;
return _16;
});
