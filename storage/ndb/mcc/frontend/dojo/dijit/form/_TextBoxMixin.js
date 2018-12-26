//>>built
define("dijit/form/_TextBoxMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom","dojo/_base/event","dojo/keys","dojo/_base/lang",".."],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_2("dijit.form._TextBoxMixin",null,{trim:false,uppercase:false,lowercase:false,propercase:false,maxLength:"",selectOnClick:false,placeHolder:"",_getValueAttr:function(){
return this.parse(this.get("displayedValue"),this.constraints);
},_setValueAttr:function(_9,_a,_b){
var _c;
if(_9!==undefined){
_c=this.filter(_9);
if(typeof _b!="string"){
if(_c!==null&&((typeof _c!="number")||!isNaN(_c))){
_b=this.filter(this.format(_c,this.constraints));
}else{
_b="";
}
}
}
if(_b!=null&&_b!=undefined&&((typeof _b)!="number"||!isNaN(_b))&&this.textbox.value!=_b){
this.textbox.value=_b;
this._set("displayedValue",this.get("displayedValue"));
}
if(this.textDir=="auto"){
this.applyTextDir(this.focusNode,_b);
}
this.inherited(arguments,[_c,_a]);
},displayedValue:"",_getDisplayedValueAttr:function(){
return this.filter(this.textbox.value);
},_setDisplayedValueAttr:function(_d){
if(_d===null||_d===undefined){
_d="";
}else{
if(typeof _d!="string"){
_d=String(_d);
}
}
this.textbox.value=_d;
this._setValueAttr(this.get("value"),undefined);
this._set("displayedValue",this.get("displayedValue"));
if(this.textDir=="auto"){
this.applyTextDir(this.focusNode,_d);
}
},format:function(_e){
return ((_e==null||_e==undefined)?"":(_e.toString?_e.toString():_e));
},parse:function(_f){
return _f;
},_refreshState:function(){
},onInput:function(){
},__skipInputEvent:false,_onInput:function(){
if(this.textDir=="auto"){
this.applyTextDir(this.focusNode,this.focusNode.value);
}
this._refreshState();
this._set("displayedValue",this.get("displayedValue"));
},postCreate:function(){
this.textbox.setAttribute("value",this.textbox.value);
this.inherited(arguments);
var _10=function(e){
var _11=e.charOrCode||e.keyCode||229;
if(e.type=="keydown"){
switch(_11){
case _5.SHIFT:
case _5.ALT:
case _5.CTRL:
case _5.META:
case _5.CAPS_LOCK:
return;
default:
if(_11>=65&&_11<=90){
return;
}
}
}
if(e.type=="keypress"&&typeof _11!="string"){
return;
}
if(e.type=="input"){
if(this.__skipInputEvent){
this.__skipInputEvent=false;
return;
}
}else{
this.__skipInputEvent=true;
}
var _12=_6.mixin({},e,{charOrCode:_11,wasConsumed:false,preventDefault:function(){
_12.wasConsumed=true;
e.preventDefault();
},stopPropagation:function(){
e.stopPropagation();
}});
if(this.onInput(_12)===false){
_4.stop(_12);
}
if(_12.wasConsumed){
return;
}
setTimeout(_6.hitch(this,"_onInput",_12),0);
};
_1.forEach(["onkeydown","onkeypress","onpaste","oncut","oninput"],function(_13){
this.connect(this.textbox,_13,_10);
},this);
},_blankValue:"",filter:function(val){
if(val===null){
return this._blankValue;
}
if(typeof val!="string"){
return val;
}
if(this.trim){
val=_6.trim(val);
}
if(this.uppercase){
val=val.toUpperCase();
}
if(this.lowercase){
val=val.toLowerCase();
}
if(this.propercase){
val=val.replace(/[^\s]+/g,function(_14){
return _14.substring(0,1).toUpperCase()+_14.substring(1);
});
}
return val;
},_setBlurValue:function(){
this._setValueAttr(this.get("value"),true);
},_onBlur:function(e){
if(this.disabled){
return;
}
this._setBlurValue();
this.inherited(arguments);
if(this._selectOnClickHandle){
this.disconnect(this._selectOnClickHandle);
}
},_isTextSelected:function(){
return this.textbox.selectionStart==this.textbox.selectionEnd;
},_onFocus:function(by){
if(this.disabled||this.readOnly){
return;
}
if(this.selectOnClick&&by=="mouse"){
this._selectOnClickHandle=this.connect(this.domNode,"onmouseup",function(){
this.disconnect(this._selectOnClickHandle);
if(this._isTextSelected()){
_8.selectInputText(this.textbox);
}
});
}
this.inherited(arguments);
this._refreshState();
},reset:function(){
this.textbox.value="";
this.inherited(arguments);
},_setTextDirAttr:function(_15){
if(!this._created||this.textDir!=_15){
this._set("textDir",_15);
this.applyTextDir(this.focusNode,this.focusNode.value);
}
}});
_8._setSelectionRange=_7._setSelectionRange=function(_16,_17,_18){
if(_16.setSelectionRange){
_16.setSelectionRange(_17,_18);
}
};
_8.selectInputText=_7.selectInputText=function(_19,_1a,_1b){
_19=_3.byId(_19);
if(isNaN(_1a)){
_1a=0;
}
if(isNaN(_1b)){
_1b=_19.value?_19.value.length:0;
}
try{
_19.focus();
_8._setSelectionRange(_19,_1a,_1b);
}
catch(e){
}
};
return _8;
});
