//>>built
define("dijit/form/_FormWidget",["dojo/_base/declare","dojo/_base/kernel","dojo/ready","../_Widget","../_CssStateMixin","../_TemplatedMixin","./_FormWidgetMixin"],function(_1,_2,_3,_4,_5,_6,_7){
if(!_2.isAsync){
_3(0,function(){
var _8=["dijit/form/_FormValueWidget"];
require(_8);
});
}
return _1("dijit.form._FormWidget",[_4,_6,_5,_7],{setDisabled:function(_9){
_2.deprecated("setDisabled("+_9+") is deprecated. Use set('disabled',"+_9+") instead.","","2.0");
this.set("disabled",_9);
},setValue:function(_a){
_2.deprecated("dijit.form._FormWidget:setValue("+_a+") is deprecated.  Use set('value',"+_a+") instead.","","2.0");
this.set("value",_a);
},getValue:function(){
_2.deprecated(this.declaredClass+"::getValue() is deprecated. Use get('value') instead.","","2.0");
return this.get("value");
},postMixInProperties:function(){
this.nameAttrSetting=this.name?("name=\""+this.name.replace(/'/g,"&quot;")+"\""):"";
this.inherited(arguments);
},_setTypeAttr:null});
});
