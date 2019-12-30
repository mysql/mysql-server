//>>built
define("dijit/form/_TextBoxMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom","dojo/sniff","dojo/keys","dojo/_base/lang","dojo/on","../main"],function(_1,_2,_3,_4,_5,_6,on,_7){
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
if(_b!=null&&((typeof _b)!="number"||!isNaN(_b))&&this.textbox.value!=_b){
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
if(_d==null){
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
return _e==null?"":(_e.toString?_e.toString():_e);
},parse:function(_f){
return _f;
},_refreshState:function(){
},onInput:function(evt){
},_onInput:function(evt){
if(this.textDir=="auto"){
this.applyTextDir(this.focusNode,this.focusNode.value);
}
this._lastInputEventValue=this.textbox.value;
this._processInput(this._lastInputProducingEvent);
delete this._lastInputProducingEvent;
if(this.intermediateChanges){
this._handleOnChange(this.get("value"),false);
}
},_processInput:function(evt){
this._refreshState();
this._set("displayedValue",this.get("displayedValue"));
},postCreate:function(){
this.textbox.setAttribute("value",this.textbox.value);
this.inherited(arguments);
function _10(e){
var _11;
if(e.type=="keydown"&&e.keyCode!=229){
_11=e.keyCode;
switch(_11){
case _5.SHIFT:
case _5.ALT:
case _5.CTRL:
case _5.META:
case _5.CAPS_LOCK:
case _5.NUM_LOCK:
case _5.SCROLL_LOCK:
return;
}
if(!e.ctrlKey&&!e.metaKey&&!e.altKey){
switch(_11){
case _5.NUMPAD_0:
case _5.NUMPAD_1:
case _5.NUMPAD_2:
case _5.NUMPAD_3:
case _5.NUMPAD_4:
case _5.NUMPAD_5:
case _5.NUMPAD_6:
case _5.NUMPAD_7:
case _5.NUMPAD_8:
case _5.NUMPAD_9:
case _5.NUMPAD_MULTIPLY:
case _5.NUMPAD_PLUS:
case _5.NUMPAD_ENTER:
case _5.NUMPAD_MINUS:
case _5.NUMPAD_PERIOD:
case _5.NUMPAD_DIVIDE:
return;
}
if((_11>=65&&_11<=90)||(_11>=48&&_11<=57)||_11==_5.SPACE){
return;
}
var _12=false;
for(var i in _5){
if(_5[i]===e.keyCode){
_12=true;
break;
}
}
if(!_12){
return;
}
}
}
_11=e.charCode>=32?String.fromCharCode(e.charCode):e.charCode;
if(!_11){
_11=(e.keyCode>=65&&e.keyCode<=90)||(e.keyCode>=48&&e.keyCode<=57)||e.keyCode==_5.SPACE?String.fromCharCode(e.keyCode):e.keyCode;
}
if(!_11){
_11=229;
}
if(e.type=="keypress"){
if(typeof _11!="string"){
return;
}
if((_11>="a"&&_11<="z")||(_11>="A"&&_11<="Z")||(_11>="0"&&_11<="9")||(_11===" ")){
if(e.ctrlKey||e.metaKey||e.altKey){
return;
}
}
}
var _13={faux:true},_14;
for(_14 in e){
if(_14!="layerX"&&_14!="layerY"){
var v=e[_14];
if(typeof v!="function"&&typeof v!="undefined"){
_13[_14]=v;
}
}
}
_6.mixin(_13,{charOrCode:_11,_wasConsumed:false,preventDefault:function(){
_13._wasConsumed=true;
e.preventDefault();
},stopPropagation:function(){
e.stopPropagation();
}});
this._lastInputProducingEvent=_13;
if(this.onInput(_13)===false){
_13.preventDefault();
_13.stopPropagation();
}
if(_13._wasConsumed){
return;
}
if(_4("ie")<=9){
switch(e.keyCode){
case _5.TAB:
case _5.ESCAPE:
case _5.DOWN_ARROW:
case _5.UP_ARROW:
case _5.LEFT_ARROW:
case _5.RIGHT_ARROW:
break;
default:
if(e.keyCode==_5.ENTER&&this.textbox.tagName.toLowerCase()!="textarea"){
break;
}
this.defer(function(){
if(this.textbox.value!==this._lastInputEventValue){
on.emit(this.textbox,"input",{bubbles:true});
}
});
}
}
};
this.own(on(this.textbox,"keydown, keypress, paste, cut, compositionend",_6.hitch(this,_10)),on(this.textbox,"input",_6.hitch(this,"_onInput")),on(this.domNode,"keypress",function(e){
e.stopPropagation();
}));
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
val=val.replace(/[^\s]+/g,function(_15){
return _15.substring(0,1).toUpperCase()+_15.substring(1);
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
},_isTextSelected:function(){
return this.textbox.selectionStart!=this.textbox.selectionEnd;
},_onFocus:function(by){
if(this.disabled||this.readOnly){
return;
}
if(this.selectOnClick&&by=="mouse"){
this._selectOnClickHandle=this.connect(this.domNode,"onmouseup",function(){
this.disconnect(this._selectOnClickHandle);
this._selectOnClickHandle=null;
if(!this._isTextSelected()){
_8.selectInputText(this.textbox);
}
});
this.defer(function(){
if(this._selectOnClickHandle){
this.disconnect(this._selectOnClickHandle);
this._selectOnClickHandle=null;
}
},500);
}
this.inherited(arguments);
this._refreshState();
},reset:function(){
this.textbox.value="";
this.inherited(arguments);
},_setTextDirAttr:function(_16){
if(!this._created||this.textDir!=_16){
this._set("textDir",_16);
this.applyTextDir(this.focusNode,this.focusNode.value);
}
}});
_8._setSelectionRange=_7._setSelectionRange=function(_17,_18,_19){
if(_17.setSelectionRange){
_17.setSelectionRange(_18,_19);
}
};
_8.selectInputText=_7.selectInputText=function(_1a,_1b,_1c){
_1a=_3.byId(_1a);
if(isNaN(_1b)){
_1b=0;
}
if(isNaN(_1c)){
_1c=_1a.value?_1a.value.length:0;
}
try{
_1a.focus();
_8._setSelectionRange(_1a,_1b,_1c);
}
catch(e){
}
};
return _8;
});
