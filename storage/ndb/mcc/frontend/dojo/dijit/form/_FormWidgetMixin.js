//>>built
define("dijit/form/_FormWidgetMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-style","dojo/_base/lang","dojo/mouse","dojo/_base/sniff","dojo/_base/window","dojo/window","../a11y"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dijit.form._FormWidgetMixin",null,{name:"",alt:"",value:"",type:"text",tabIndex:"0",_setTabIndexAttr:"focusNode",disabled:false,intermediateChanges:false,scrollOnFocus:true,_setIdAttr:"focusNode",postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmousedown","_onMouseDown");
},_setDisabledAttr:function(_b){
this._set("disabled",_b);
_3.set(this.focusNode,"disabled",_b);
if(this.valueNode){
_3.set(this.valueNode,"disabled",_b);
}
this.focusNode.setAttribute("aria-disabled",_b);
if(_b){
this._set("hovering",false);
this._set("active",false);
var _c="tabIndex" in this.attributeMap?this.attributeMap.tabIndex:("_setTabIndexAttr" in this)?this._setTabIndexAttr:"focusNode";
_1.forEach(_5.isArray(_c)?_c:[_c],function(_d){
var _e=this[_d];
if(_7("webkit")||_a.hasDefaultTabStop(_e)){
_e.setAttribute("tabIndex","-1");
}else{
_e.removeAttribute("tabIndex");
}
},this);
}else{
if(this.tabIndex!=""){
this.set("tabIndex",this.tabIndex);
}
}
},_onFocus:function(e){
if(this.scrollOnFocus){
_9.scrollIntoView(this.domNode);
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
},compare:function(_f,_10){
if(typeof _f=="number"&&typeof _10=="number"){
return (isNaN(_f)&&isNaN(_10))?0:_f-_10;
}else{
if(_f>_10){
return 1;
}else{
if(_f<_10){
return -1;
}else{
return 0;
}
}
}
},onChange:function(){
},_onChangeActive:false,_handleOnChange:function(_11,_12){
if(this._lastValueReported==undefined&&(_12===null||!this._onChangeActive)){
this._resetValue=this._lastValueReported=_11;
}
this._pendingOnChange=this._pendingOnChange||(typeof _11!=typeof this._lastValueReported)||(this.compare(_11,this._lastValueReported)!=0);
if((this.intermediateChanges||_12||_12===undefined)&&this._pendingOnChange){
this._lastValueReported=_11;
this._pendingOnChange=false;
if(this._onChangeActive){
if(this._onChangeHandle){
clearTimeout(this._onChangeHandle);
}
this._onChangeHandle=setTimeout(_5.hitch(this,function(){
this._onChangeHandle=null;
this.onChange(_11);
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
if((!this.focused||!_7("ie"))&&!e.ctrlKey&&_6.isLeft(e)&&this.isFocusable()){
var _13=this.connect(_8.body(),"onmouseup",function(){
if(this.isFocusable()){
this.focus();
}
this.disconnect(_13);
});
}
}});
});
