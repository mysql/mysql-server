//>>built
define("dojox/mvc/_TextBoxExtensions",["dojo/_base/lang","dijit/_WidgetBase","dijit/form/ValidationTextBox","dijit/form/NumberTextBox"],function(_1,_2,_3,_4){
var _5=_3.prototype.isValid;
_3.prototype.isValid=function(_6){
return (this.inherited("isValid",arguments)!==false&&_5.apply(this,[_6]));
};
var _7=_4.prototype.isValid;
_4.prototype.isValid=function(_8){
return (this.inherited("isValid",arguments)!==false&&_7.apply(this,[_8]));
};
if(!_1.isFunction(_2.prototype.isValid)){
_2.prototype.isValid=function(){
var _9=this.get("valid");
return typeof _9=="undefined"?true:_9;
};
}
_2.prototype._setValidAttr=function(_a){
this._set("valid",_a);
this.validate();
};
});
