//>>built
define("dijit/form/_ToggleButtonMixin",["dojo/_base/declare","dojo/dom-attr"],function(_1,_2){
return _1("dijit.form._ToggleButtonMixin",null,{checked:false,_aria_attr:"aria-pressed",_onClick:function(_3){
var _4=this.checked;
this._set("checked",!_4);
var _5=this.inherited(arguments);
this.set("checked",_5?this.checked:_4);
return _5;
},_setCheckedAttr:function(_6,_7){
this._set("checked",_6);
_2.set(this.focusNode||this.domNode,"checked",_6);
(this.focusNode||this.domNode).setAttribute(this._aria_attr,_6?"true":"false");
this._handleOnChange(_6,_7);
},reset:function(){
this._hasBeenBlurred=false;
this.set("checked",this.params.checked||false);
}});
});
