//>>built
require({cache:{"url:dojox/form/resources/FilePickerTextBox.html":"<div class=\"dijit dijitReset dijitInlineTable dijitLeft\"\n\tid=\"widget_${id}\"\n\trole=\"combobox\" tabIndex=\"-1\"\n\t><div style=\"overflow:hidden;\"\n\t\t><div class='dijitReset dijitRight dijitButtonNode dijitArrowButton dijitDownArrowButton'\n\t\t\tdojoAttachPoint=\"downArrowNode,_buttonNode,_popupStateNode\" role=\"presentation\"\n\t\t\t><div class=\"dijitArrowButtonInner\">&thinsp;</div\n\t\t\t><div class=\"dijitArrowButtonChar\">&#9660;</div\n\t\t></div\n\t\t><div class=\"dijitReset dijitValidationIcon\"><br></div\n\t\t><div class=\"dijitReset dijitValidationIconText\">&Chi;</div\n\t\t><div class=\"dijitReset dijitInputField\"\n\t\t\t><input type=\"text\" autocomplete=\"off\" ${!nameAttrSetting} class='dijitReset'\n\t\t\t\tdojoAttachEvent='onkeypress:_onKey' \n\t\t\t\tdojoAttachPoint='textbox,focusNode' role=\"textbox\" aria-haspopup=\"true\" aria-autocomplete=\"list\"\n\t\t/></div\n\t></div\n></div>\n"}});
define("dojox/form/FilePickerTextBox",["dojo/_base/lang","dojo/_base/array","dojo/_base/event","dojo/window","dijit/focus","dijit/registry","dijit/form/_TextBoxMixin","dijit/form/ValidationTextBox","dijit/_HasDropDown","dojox/widget/FilePicker","dojo/text!./resources/FilePickerTextBox.html","dojo/_base/declare","dojo/keys"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
return _c("dojox.form.FilePickerTextBox",[_8,_9],{baseClass:"dojoxFilePickerTextBox",templateString:_b,searchDelay:500,valueItem:null,numPanes:2.25,postMixInProperties:function(){
this.inherited(arguments);
this.dropDown=new _a(this.constraints);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.dropDown,"onChange",this._onWidgetChange);
this.connect(this.focusNode,"onblur","_focusBlur");
this.connect(this.focusNode,"onfocus","_focusFocus");
this.connect(this.focusNode,"ondblclick",function(){
_7.selectInputText(this.focusNode);
});
},_setValueAttr:function(_e,_f,_10){
if(!this._searchInProgress){
this.inherited(arguments);
_e=_e||"";
var _11=this.dropDown.get("pathValue")||"";
if(_e!==_11){
this._skip=true;
var fx=_1.hitch(this,"_setBlurValue");
this.dropDown._setPathValueAttr(_e,!_10,this._settingBlurValue?fx:null);
}
}
},_onWidgetChange:function(_12){
if(!_12&&this.focusNode.value){
this._hasValidPath=false;
this.focusNode.value="";
}else{
this.valueItem=_12;
var _13=this.dropDown._getPathValueAttr(_12);
if(_13){
this._hasValidPath=true;
}
if(!this._skip){
this._setValueAttr(_13,undefined,true);
}
delete this._skip;
}
this.validate();
},startup:function(){
if(!this.dropDown._started){
this.dropDown.startup();
}
this.inherited(arguments);
},openDropDown:function(){
this.dropDown.domNode.style.width="0px";
if(!("minPaneWidth" in (this.constraints||{}))){
this.dropDown.set("minPaneWidth",(this.domNode.offsetWidth/this.numPanes));
}
this.inherited(arguments);
},toggleDropDown:function(){
this.inherited(arguments);
if(this._opened){
this.dropDown.set("pathValue",this.get("value"));
}
},_focusBlur:function(e){
if(e.explicitOriginalTarget==this.focusNode&&!this._allowBlur){
window.setTimeout(_1.hitch(this,function(){
if(!this._allowBlur){
this.focus();
}
}),1);
}else{
if(this._menuFocus){
this.dropDown._updateClass(this._menuFocus,"Item",{"Hover":false});
delete this._menuFocus;
}
}
},_focusFocus:function(e){
if(this._menuFocus){
this.dropDown._updateClass(this._menuFocus,"Item",{"Hover":false});
}
delete this._menuFocus;
var _14=_5.curNode;
if(_14){
_14=_6.byNode(_14);
if(_14){
this._menuFocus=_14.domNode;
}
}
if(this._menuFocus){
this.dropDown._updateClass(this._menuFocus,"Item",{"Hover":true});
}
delete this._allowBlur;
},_onBlur:function(){
this._allowBlur=true;
delete this.dropDown._savedFocus;
this.inherited(arguments);
},_setBlurValue:function(){
if(this.dropDown&&!this._settingBlurValue){
this._settingBlurValue=true;
this.set("value",this.focusNode.value);
}else{
delete this._settingBlurValue;
this.inherited(arguments);
}
},parse:function(_15,_16){
if(this._hasValidPath||this._hasSelection){
return _15;
}
var dd=this.dropDown,_17=dd.topDir,sep=dd.pathSeparator;
var _18=dd.get("pathValue");
var _19=function(v){
if(_17.length&&v.indexOf(_17)===0){
v=v.substring(_17.length);
}
if(sep&&v[v.length-1]==sep){
v=v.substring(0,v.length-1);
}
return v;
};
_18=_19(_18);
var val=_19(_15);
if(val==_18){
return _15;
}
return undefined;
},_startSearchFromInput:function(){
var dd=this.dropDown,fn=this.focusNode;
var val=fn.value,_1a=val,_1b=dd.topDir;
if(this._hasSelection){
_7.selectInputText(fn,_1a.length);
}
this._hasSelection=false;
if(_1b.length&&val.indexOf(_1b)===0){
val=val.substring(_1b.length);
}
var _1c=val.split(dd.pathSeparator);
var _1d=_1.hitch(this,function(idx){
var dir=_1c[idx];
var _1e=dd.getChildren()[idx];
var _1f;
this._searchInProgress=true;
var _20=_1.hitch(this,function(){
delete this._searchInProgress;
});
if((dir||_1e)&&!this._opened){
this.toggleDropDown();
}
if(dir&&_1e){
var fx=_1.hitch(this,function(){
if(_1f){
this.disconnect(_1f);
}
delete _1f;
var _21=_1e._menu.getChildren();
var _22=_2.filter(_21,function(i){
return i.label==dir;
})[0];
var _23=_2.filter(_21,function(i){
return (i.label.indexOf(dir)===0);
})[0];
if(_22&&((_1c.length>idx+1&&_22.children)||(!_22.children))){
idx++;
_1e._menu.onItemClick(_22,{type:"internal",stopPropagation:function(){
},preventDefault:function(){
}});
if(_1c[idx]){
_1d(idx);
}else{
_20();
}
}else{
_1e._setSelected(null);
if(_23&&_1c.length===idx+1){
dd._setInProgress=true;
dd._removeAfter(_1e);
delete dd._setInProgress;
var _24=_23.label;
if(_23.children){
_24+=dd.pathSeparator;
}
_24=_24.substring(dir.length);
window.setTimeout(function(){
_4.scrollIntoView(_23.domNode);
},1);
fn.value=_1a+_24;
_7.selectInputText(fn,_1a.length);
this._hasSelection=true;
try{
_23.focusNode.focus();
}
catch(e){
}
}else{
if(this._menuFocus){
this.dropDown._updateClass(this._menuFocus,"Item",{"Hover":false,"Focus":false});
}
delete this._menuFocus;
}
_20();
}
});
if(!_1e.isLoaded){
_1f=this.connect(_1e,"onLoad",fx);
}else{
fx();
}
}else{
if(_1e){
_1e._setSelected(null);
dd._setInProgress=true;
dd._removeAfter(_1e);
delete dd._setInProgress;
}
_20();
}
});
_1d(0);
},_onKey:function(e){
if(this.disabled||this.readOnly){
return;
}
var c=e.charOrCode;
if(c==_d.DOWN_ARROW){
this._allowBlur=true;
}
if(c==_d.ENTER&&this._opened){
this.dropDown.onExecute();
_7.selectInputText(this.focusNode,this.focusNode.value.length);
this._hasSelection=false;
_3.stop(e);
return;
}
if((c==_d.RIGHT_ARROW||c==_d.LEFT_ARROW||c==_d.TAB)&&this._hasSelection){
this._startSearchFromInput();
_3.stop(e);
return;
}
this.inherited(arguments);
var _25=false;
if((c==_d.BACKSPACE||c==_d.DELETE)&&this._hasSelection){
this._hasSelection=false;
}else{
if(c==_d.BACKSPACE||c==_d.DELETE||c==" "){
_25=true;
}else{
_25=e.keyChar!=="";
}
}
if(this._searchTimer){
window.clearTimeout(this._searchTimer);
}
delete this._searchTimer;
if(_25){
this._hasValidPath=false;
this._hasSelection=false;
this._searchTimer=window.setTimeout(_1.hitch(this,"_startSearchFromInput"),this.searchDelay+1);
}
}});
});
