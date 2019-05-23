//>>built
define("dijit/form/_CheckBoxMixin",["dojo/_base/declare","dojo/dom-attr","dojo/_base/event"],function(_1,_2,_3){
return _1("dijit.form._CheckBoxMixin",null,{type:"checkbox",value:"on",readOnly:false,_aria_attr:"aria-checked",_setReadOnlyAttr:function(_4){
this._set("readOnly",_4);
_2.set(this.focusNode,"readOnly",_4);
},_setLabelAttr:undefined,_getSubmitValue:function(_5){
return (_5==null||_5==="")?"on":_5;
},_setValueAttr:function(_6){
_6=this._getSubmitValue(_6);
this._set("value",_6);
_2.set(this.focusNode,"value",_6);
},reset:function(){
this.inherited(arguments);
this._set("value",this._getSubmitValue(this.params.value));
_2.set(this.focusNode,"value",this.value);
},_onClick:function(e){
if(this.readOnly){
_3.stop(e);
return false;
}
return this.inherited(arguments);
}});
});
