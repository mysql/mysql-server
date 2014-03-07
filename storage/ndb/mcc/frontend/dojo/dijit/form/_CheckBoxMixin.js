//>>built
define("dijit/form/_CheckBoxMixin",["dojo/_base/declare","dojo/dom-attr","dojo/_base/event"],function(_1,_2,_3){
return _1("dijit.form._CheckBoxMixin",null,{type:"checkbox",value:"on",readOnly:false,_aria_attr:"aria-checked",_setReadOnlyAttr:function(_4){
this._set("readOnly",_4);
_2.set(this.focusNode,"readOnly",_4);
this.focusNode.setAttribute("aria-readonly",_4);
},_setLabelAttr:undefined,postMixInProperties:function(){
if(this.value==""){
this.value="on";
}
this.inherited(arguments);
},reset:function(){
this.inherited(arguments);
this._set("value",this.params.value||"on");
_2.set(this.focusNode,"value",this.value);
},_onClick:function(e){
if(this.readOnly){
_3.stop(e);
return false;
}
return this.inherited(arguments);
}});
});
