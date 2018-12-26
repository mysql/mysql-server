//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/window,dijit/_WidgetBase,dijit/focus"],function(_1,_2,_3){
_2.provide("dojox.mobile.app._FormWidget");
_2.experimental("dojox.mobile.app._FormWidget");
_2.require("dojo.window");
_2.require("dijit._WidgetBase");
_2.require("dijit.focus");
_2.declare("dojox.mobile.app._FormWidget",_1._WidgetBase,{name:"",alt:"",value:"",type:"text",disabled:false,intermediateChanges:false,scrollOnFocus:false,attributeMap:_2.delegate(_1._WidgetBase.prototype.attributeMap,{value:"focusNode",id:"focusNode",alt:"focusNode",title:"focusNode"}),postMixInProperties:function(){
this.nameAttrSetting=this.name?("name=\""+this.name.replace(/'/g,"&quot;")+"\""):"";
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmousedown","_onMouseDown");
},_setDisabledAttr:function(_4){
this.disabled=_4;
_2.attr(this.focusNode,"disabled",_4);
if(this.valueNode){
_2.attr(this.valueNode,"disabled",_4);
}
},_onFocus:function(e){
if(this.scrollOnFocus){
_2.window.scrollIntoView(this.domNode);
}
this.inherited(arguments);
},isFocusable:function(){
return !this.disabled&&!this.readOnly&&this.focusNode&&(_2.style(this.domNode,"display")!="none");
},focus:function(){
this.focusNode.focus();
},compare:function(_5,_6){
if(typeof _5=="number"&&typeof _6=="number"){
return (isNaN(_5)&&isNaN(_6))?0:_5-_6;
}else{
if(_5>_6){
return 1;
}else{
if(_5<_6){
return -1;
}else{
return 0;
}
}
}
},onChange:function(_7){
},_onChangeActive:false,_handleOnChange:function(_8,_9){
this._lastValue=_8;
if(this._lastValueReported==undefined&&(_9===null||!this._onChangeActive)){
this._resetValue=this._lastValueReported=_8;
}
if((this.intermediateChanges||_9||_9===undefined)&&((typeof _8!=typeof this._lastValueReported)||this.compare(_8,this._lastValueReported)!=0)){
this._lastValueReported=_8;
if(this._onChangeActive){
if(this._onChangeHandle){
clearTimeout(this._onChangeHandle);
}
this._onChangeHandle=setTimeout(_2.hitch(this,function(){
this._onChangeHandle=null;
this.onChange(_8);
}),0);
}
}
},create:function(){
this.inherited(arguments);
this._onChangeActive=true;
},destroy:function(){
if(this._onChangeHandle){
clearTimeout(this._onChangeHandle);
this.onChange(this._lastValueReported);
}
this.inherited(arguments);
},_onMouseDown:function(e){
if(this.isFocusable()){
var _a=this.connect(_2.body(),"onmouseup",function(){
if(this.isFocusable()){
this.focus();
}
this.disconnect(_a);
});
}
},selectInputText:function(_b,_c,_d){
var _e=_2.global;
var _f=_2.doc;
_b=_2.byId(_b);
if(isNaN(_c)){
_c=0;
}
if(isNaN(_d)){
_d=_b.value?_b.value.length:0;
}
_1.focus(_b);
if(_e["getSelection"]&&_b.setSelectionRange){
_b.setSelectionRange(_c,_d);
}
}});
_2.declare("dojox.mobile.app._FormValueWidget",_3.mobile.app._FormWidget,{readOnly:false,attributeMap:_2.delegate(_3.mobile.app._FormWidget.prototype.attributeMap,{value:"",readOnly:"focusNode"}),_setReadOnlyAttr:function(_10){
this.readOnly=_10;
_2.attr(this.focusNode,"readOnly",_10);
},postCreate:function(){
this.inherited(arguments);
if(this._resetValue===undefined){
this._resetValue=this.value;
}
},_setValueAttr:function(_11,_12){
this.value=_11;
this._handleOnChange(_11,_12);
},_getValueAttr:function(){
return this._lastValue;
},undo:function(){
this._setValueAttr(this._lastValueReported,false);
},reset:function(){
this._hasBeenBlurred=false;
this._setValueAttr(this._resetValue,true);
}});
});
