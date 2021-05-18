//>>built
define("dijit/form/_FormWidgetMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-style","dojo/_base/lang","dojo/mouse","dojo/on","dojo/sniff","dojo/window","../a11y"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9){
return _2("dijit.form._FormWidgetMixin",null,{name:"",alt:"",value:"",type:"text","aria-label":"focusNode",tabIndex:"0",_setTabIndexAttr:"focusNode",disabled:false,intermediateChanges:false,scrollOnFocus:true,_setIdAttr:"focusNode",_setDisabledAttr:function(_a){
this._set("disabled",_a);
if(/^(button|input|select|textarea|optgroup|option|fieldset)$/i.test(this.focusNode.tagName)){
_3.set(this.focusNode,"disabled",_a);
if(_7("trident")&&"readOnly" in this){
_3.set(this.focusNode,"readonly",_a||this.readOnly);
}
}else{
this.focusNode.setAttribute("aria-disabled",_a?"true":"false");
}
if(this.valueNode){
_3.set(this.valueNode,"disabled",_a);
}
if(_a){
this._set("hovering",false);
this._set("active",false);
var _b="tabIndex" in this.attributeMap?this.attributeMap.tabIndex:("_setTabIndexAttr" in this)?this._setTabIndexAttr:"focusNode";
_1.forEach(_5.isArray(_b)?_b:[_b],function(_c){
var _d=this[_c];
if(_7("webkit")||_9.hasDefaultTabStop(_d)){
_d.setAttribute("tabIndex","-1");
}else{
_d.removeAttribute("tabIndex");
}
},this);
}else{
if(this.tabIndex!=""){
this.set("tabIndex",this.tabIndex);
}
}
},_onFocus:function(by){
if(by=="mouse"&&this.isFocusable()){
var _e=this.own(on(this.focusNode,"focus",function(){
_f.remove();
_e.remove();
}))[0];
var _10=_7("pointer-events")?"pointerup":_7("MSPointer")?"MSPointerUp":_7("touch-events")?"touchend, mouseup":"mouseup";
var _f=this.own(on(this.ownerDocumentBody,_10,_5.hitch(this,function(evt){
_f.remove();
_e.remove();
if(this.focused){
if(evt.type=="touchend"){
this.defer("focus");
}else{
this.focus();
}
}
})))[0];
}
if(this.scrollOnFocus){
this.defer(function(){
_8.scrollIntoView(this.domNode);
});
}
this.inherited(arguments);
},isFocusable:function(){
return !this.disabled&&this.focusNode&&(_4.get(this.domNode,"display")!="none");
},focus:function(){
if(!this.disabled&&this.focusNode.focus){
try{
this.focusNode.focus();
}
catch(e){
}
}
},compare:function(_11,_12){
if(typeof _11=="number"&&typeof _12=="number"){
return (isNaN(_11)&&isNaN(_12))?0:_11-_12;
}else{
if(_11>_12){
return 1;
}else{
if(_11<_12){
return -1;
}else{
return 0;
}
}
}
},onChange:function(){
},_onChangeActive:false,_handleOnChange:function(_13,_14){
if(this._lastValueReported==undefined&&(_14===null||!this._onChangeActive)){
this._resetValue=this._lastValueReported=_13;
}
this._pendingOnChange=this._pendingOnChange||(typeof _13!=typeof this._lastValueReported)||(this.compare(_13,this._lastValueReported)!=0);
if((this.intermediateChanges||_14||_14===undefined)&&this._pendingOnChange){
this._lastValueReported=_13;
this._pendingOnChange=false;
if(this._onChangeActive){
if(this._onChangeHandle){
this._onChangeHandle.remove();
}
this._onChangeHandle=this.defer(function(){
this._onChangeHandle=null;
this.onChange(_13);
});
}
}
},create:function(){
this.inherited(arguments);
this._onChangeActive=true;
},destroy:function(){
if(this._onChangeHandle){
this._onChangeHandle.remove();
this.onChange(this._lastValueReported);
}
this.inherited(arguments);
}});
});
