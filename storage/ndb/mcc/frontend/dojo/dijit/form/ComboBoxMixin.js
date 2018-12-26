//>>built
require({cache:{"url:dijit/form/templates/DropDownBox.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\"\n\trole=\"combobox\"\n\t><div class='dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton dijitArrowButtonContainer'\n\t\tdata-dojo-attach-point=\"_buttonNode, _popupStateNode\" role=\"presentation\"\n\t\t><input class=\"dijitReset dijitInputField dijitArrowButtonInner\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t${_buttonInputDisabled}\n\t/></div\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class='dijitReset dijitInputInner' ${!nameAttrSetting} type=\"text\" autocomplete=\"off\"\n\t\t\tdata-dojo-attach-point=\"textbox,focusNode\" role=\"textbox\" aria-haspopup=\"true\"\n\t/></div\n></div>\n"}});
define("dijit/form/ComboBoxMixin",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/kernel","dojo/_base/lang","dojo/store/util/QueryResults","./_AutoCompleterMixin","./_ComboBoxMenu","../_HasDropDown","dojo/text!./templates/DropDownBox.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dijit.form.ComboBoxMixin",[_8,_6],{dropDownClass:_7,hasDownArrow:true,templateString:_9,baseClass:"dijitTextBox dijitComboBox",cssStateNodes:{"_buttonNode":"dijitDownArrowButton"},_setHasDownArrowAttr:function(_a){
this._set("hasDownArrow",_a);
this._buttonNode.style.display=_a?"":"none";
},_showResultList:function(){
this.displayMessage("");
this.inherited(arguments);
},_setStoreAttr:function(_b){
if(!_b.get){
_4.mixin(_b,{_oldAPI:true,get:function(id){
var _c=new _2();
this.fetchItemByIdentity({identity:id,onItem:function(_d){
_c.resolve(_d);
},onError:function(_e){
_c.reject(_e);
}});
return _c.promise;
},query:function(_f,_10){
var _11=new _2(function(){
_12.abort&&_12.abort();
});
var _12=this.fetch(_4.mixin({query:_f,onBegin:function(_13){
_11.total=_13;
},onComplete:function(_14){
_11.resolve(_14);
},onError:function(_15){
_11.reject(_15);
}},_10));
return _5(_11);
}});
}
this._set("store",_b);
},postMixInProperties:function(){
if(this.params.store){
this._setStoreAttr(this.params.store);
}
this.inherited(arguments);
if(!this.params.store){
var _16=this.declaredClass;
_4.mixin(this.store,{getValue:function(_17,_18){
_3.deprecated(_16+".store.getValue(item, attr) is deprecated for builtin store.  Use item.attr directly","","2.0");
return _17[_18];
},getLabel:function(_19){
_3.deprecated(_16+".store.getLabel(item) is deprecated for builtin store.  Use item.label directly","","2.0");
return _19.name;
},fetch:function(_1a){
_3.deprecated(_16+".store.fetch() is deprecated for builtin store.","Use store.query()","2.0");
var _1b=["dojo/data/ObjectStore"];
require(_1b,_4.hitch(this,function(_1c){
new _1c({objectStore:this}).fetch(_1a);
}));
}});
}
}});
});
