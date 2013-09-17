//>>built
define("dijit/form/FilteringSelect",["dojo/data/util/filter","dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","./MappedTextBox","./ComboBoxMixin"],function(_1,_2,_3,_4,_5,_6){
return _2("dijit.form.FilteringSelect",[_5,_6],{required:true,_lastDisplayedValue:"",_isValidSubset:function(){
return this._opened;
},isValid:function(){
return this.item||(!this.required&&this.get("displayedValue")=="");
},_refreshState:function(){
if(!this.searchTimer){
this.inherited(arguments);
}
},_callbackSetLabel:function(_7,_8,_9,_a){
if((_8&&_8[this.searchAttr]!==this._lastQuery)||(!_8&&_7.length&&this.store.getIdentity(_7[0])!=this._lastQuery)){
return;
}
if(!_7.length){
this.set("value","",_a||(_a===undefined&&!this.focused),this.textbox.value,null);
}else{
this.set("item",_7[0],_a);
}
},_openResultList:function(_b,_c,_d){
if(_c[this.searchAttr]!==this._lastQuery){
return;
}
this.inherited(arguments);
if(this.item===undefined){
this.validate(true);
}
},_getValueAttr:function(){
return this.valueNode.value;
},_getValueField:function(){
return "value";
},_setValueAttr:function(_e,_f,_10,_11){
if(!this._onChangeActive){
_f=null;
}
if(_11===undefined){
if(_e===null||_e===""){
_e="";
if(!_4.isString(_10)){
this._setDisplayedValueAttr(_10||"",_f);
return;
}
}
var _12=this;
this._lastQuery=_e;
_3.when(this.store.get(_e),function(_13){
_12._callbackSetLabel(_13?[_13]:[],undefined,undefined,_f);
});
}else{
this.valueNode.value=_e;
this.inherited(arguments);
}
},_setItemAttr:function(_14,_15,_16){
this.inherited(arguments);
this._lastDisplayedValue=this.textbox.value;
},_getDisplayQueryString:function(_17){
return _17.replace(/([\\\*\?])/g,"\\$1");
},_setDisplayedValueAttr:function(_18,_19){
if(_18==null){
_18="";
}
if(!this._created){
if(!("displayedValue" in this.params)){
return;
}
_19=false;
}
if(this.store){
this.closeDropDown();
var _1a=_4.clone(this.query);
var qs=this._getDisplayQueryString(_18),q;
if(this.store._oldAPI){
q=qs;
}else{
q=_1.patternToRegExp(qs,this.ignoreCase);
q.toString=function(){
return qs;
};
}
this._lastQuery=_1a[this.searchAttr]=q;
this.textbox.value=_18;
this._lastDisplayedValue=_18;
this._set("displayedValue",_18);
var _1b=this;
var _1c={ignoreCase:this.ignoreCase,deep:true};
_4.mixin(_1c,this.fetchProperties);
this._fetchHandle=this.store.query(_1a,_1c);
_3.when(this._fetchHandle,function(_1d){
_1b._fetchHandle=null;
_1b._callbackSetLabel(_1d||[],_1a,_1c,_19);
},function(err){
_1b._fetchHandle=null;
if(!_1b._cancelingQuery){
console.error("dijit.form.FilteringSelect: "+err.toString());
}
});
}
},undo:function(){
this.set("displayedValue",this._lastDisplayedValue);
}});
});
