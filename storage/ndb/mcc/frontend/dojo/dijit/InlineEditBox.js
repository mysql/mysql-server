//>>built
require({cache:{"url:dijit/templates/InlineEditBox.html":"<span data-dojo-attach-point=\"editNode\" role=\"presentation\" class=\"dijitReset dijitInline dijitOffScreen\"\n\tdata-dojo-attach-event=\"onkeypress: _onKeyPress\"\n\t><span data-dojo-attach-point=\"editorPlaceholder\"></span\n\t><span data-dojo-attach-point=\"buttonContainer\"\n\t\t><button data-dojo-type=\"dijit/form/Button\" data-dojo-props=\"label: '${buttonSave}', 'class': 'saveButton'\"\n\t\t\tdata-dojo-attach-point=\"saveButton\" data-dojo-attach-event=\"onClick:save\"></button\n\t\t><button data-dojo-type=\"dijit/form/Button\"  data-dojo-props=\"label: '${buttonCancel}', 'class': 'cancelButton'\"\n\t\t\tdata-dojo-attach-point=\"cancelButton\" data-dojo-attach-event=\"onClick:cancel\"></button\n\t></span\n></span>\n"}});
define("dijit/InlineEditBox",["require","dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/_base/event","dojo/i18n","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/when","./focus","./_Widget","./_TemplatedMixin","./_WidgetsInTemplateMixin","./_Container","./form/Button","./form/_TextBoxMixin","./form/TextBox","dojo/text!./templates/InlineEditBox.html","dojo/i18n!./nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,fm,_f,_10,_11,_12,_13,_14,_15,_16){
var _17=_3("dijit._InlineEditor",[_f,_10,_11],{templateString:_16,postMixInProperties:function(){
this.inherited(arguments);
this.messages=_9.getLocalization("dijit","common",this.lang);
_2.forEach(["buttonSave","buttonCancel"],function(_18){
if(!this[_18]){
this[_18]=this.messages[_18];
}
},this);
},buildRendering:function(){
this.inherited(arguments);
var Cls=typeof this.editor=="string"?(_c.getObject(this.editor)||_1(this.editor)):this.editor;
var _19=this.sourceStyle,_1a="line-height:"+_19.lineHeight+";",_1b=_7.getComputedStyle(this.domNode);
_2.forEach(["Weight","Family","Size","Style"],function(_1c){
var _1d=_19["font"+_1c],_1e=_1b["font"+_1c];
if(_1e!=_1d){
_1a+="font-"+_1c+":"+_19["font"+_1c]+";";
}
},this);
_2.forEach(["marginTop","marginBottom","marginLeft","marginRight","position","left","top","right","bottom","float","clear","display"],function(_1f){
this.domNode.style[_1f]=_19[_1f];
},this);
var _20=this.inlineEditBox.width;
if(_20=="100%"){
_1a+="width:100%;";
this.domNode.style.display="block";
}else{
_1a+="width:"+(_20+(Number(_20)==_20?"px":""))+";";
}
var _21=_c.delegate(this.inlineEditBox.editorParams,{style:_1a,dir:this.dir,lang:this.lang,textDir:this.textDir});
this.editWidget=new Cls(_21,this.editorPlaceholder);
if(this.inlineEditBox.autoSave){
_6.destroy(this.buttonContainer);
}
},postCreate:function(){
this.inherited(arguments);
var ew=this.editWidget;
if(this.inlineEditBox.autoSave){
this.connect(ew,"onChange","_onChange");
this.connect(ew,"onKeyPress","_onKeyPress");
}else{
if("intermediateChanges" in ew){
ew.set("intermediateChanges",true);
this.connect(ew,"onChange","_onIntermediateChange");
this.saveButton.set("disabled",true);
}
}
},startup:function(){
this.editWidget.startup();
this.inherited(arguments);
},_onIntermediateChange:function(){
this.saveButton.set("disabled",(this.getValue()==this._resetValue)||!this.enableSave());
},destroy:function(){
this.editWidget.destroy(true);
this.inherited(arguments);
},getValue:function(){
var ew=this.editWidget;
return String(ew.get(("displayedValue" in ew||"_getDisplayedValueAttr" in ew)?"displayedValue":"value"));
},_onKeyPress:function(e){
if(this.inlineEditBox.autoSave&&this.inlineEditBox.editing){
if(e.altKey||e.ctrlKey){
return;
}
if(e.charOrCode==_b.ESCAPE){
_8.stop(e);
this.cancel(true);
}else{
if(e.charOrCode==_b.ENTER&&e.target.tagName=="INPUT"){
_8.stop(e);
this._onChange();
}
}
}
},_onBlur:function(){
this.inherited(arguments);
if(this.inlineEditBox.autoSave&&this.inlineEditBox.editing){
if(this.getValue()==this._resetValue){
this.cancel(false);
}else{
if(this.enableSave()){
this.save(false);
}
}
}
},_onChange:function(){
if(this.inlineEditBox.autoSave&&this.inlineEditBox.editing&&this.enableSave()){
fm.focus(this.inlineEditBox.displayNode);
}
},enableSave:function(){
return this.editWidget.isValid?this.editWidget.isValid():true;
},focus:function(){
this.editWidget.focus();
if(this.editWidget.focusNode){
fm._onFocusNode(this.editWidget.focusNode);
if(this.editWidget.focusNode.tagName=="INPUT"){
this.defer(function(){
_14.selectInputText(this.editWidget.focusNode);
});
}
}
}});
var _22=_3("dijit.InlineEditBox",_f,{editing:false,autoSave:true,buttonSave:"",buttonCancel:"",renderAsHtml:false,editor:_15,editorWrapper:_17,editorParams:{},disabled:false,onChange:function(){
},onCancel:function(){
},width:"100%",value:"",noValueIndicator:_d("ie")<=6?"<span style='font-family: wingdings; text-decoration: underline;'>&#160;&#160;&#160;&#160;&#x270d;&#160;&#160;&#160;&#160;</span>":"<span style='text-decoration: underline;'>&#160;&#160;&#160;&#160;&#x270d;&#160;&#160;&#160;&#160;</span>",constructor:function(){
this.editorParams={};
},postMixInProperties:function(){
this.inherited(arguments);
this.displayNode=this.srcNodeRef;
var _23={ondijitclick:"_onClick",onmouseover:"_onMouseOver",onmouseout:"_onMouseOut",onfocus:"_onMouseOver",onblur:"_onMouseOut"};
for(var _24 in _23){
this.connect(this.displayNode,_24,_23[_24]);
}
this.displayNode.setAttribute("role","button");
if(!this.displayNode.getAttribute("tabIndex")){
this.displayNode.setAttribute("tabIndex",0);
}
if(!this.value&&!("value" in this.params)){
this.value=_c.trim(this.renderAsHtml?this.displayNode.innerHTML:(this.displayNode.innerText||this.displayNode.textContent||""));
}
if(!this.value){
this.displayNode.innerHTML=this.noValueIndicator;
}
_5.add(this.displayNode,"dijitInlineEditBoxDisplayMode");
},setDisabled:function(_25){
_a.deprecated("dijit.InlineEditBox.setDisabled() is deprecated.  Use set('disabled', bool) instead.","","2.0");
this.set("disabled",_25);
},_setDisabledAttr:function(_26){
this.domNode.setAttribute("aria-disabled",_26?"true":"false");
if(_26){
this.displayNode.removeAttribute("tabIndex");
}else{
this.displayNode.setAttribute("tabIndex",0);
}
_5.toggle(this.displayNode,"dijitInlineEditBoxDisplayModeDisabled",_26);
this._set("disabled",_26);
},_onMouseOver:function(){
if(!this.disabled){
_5.add(this.displayNode,"dijitInlineEditBoxDisplayModeHover");
}
},_onMouseOut:function(){
_5.remove(this.displayNode,"dijitInlineEditBoxDisplayModeHover");
},_onClick:function(e){
if(this.disabled){
return;
}
if(e){
_8.stop(e);
}
this._onMouseOut();
this.defer("edit");
},edit:function(){
if(this.disabled||this.editing){
return;
}
this._set("editing",true);
this._savedTabIndex=_4.get(this.displayNode,"tabIndex")||"0";
if(!this.wrapperWidget){
var _27=_6.create("span",null,this.domNode,"before");
var Ewc=typeof this.editorWrapper=="string"?_c.getObject(this.editorWrapper):this.editorWrapper;
this.wrapperWidget=new Ewc({value:this.value,buttonSave:this.buttonSave,buttonCancel:this.buttonCancel,dir:this.dir,lang:this.lang,tabIndex:this._savedTabIndex,editor:this.editor,inlineEditBox:this,sourceStyle:_7.getComputedStyle(this.displayNode),save:_c.hitch(this,"save"),cancel:_c.hitch(this,"cancel"),textDir:this.textDir},_27);
if(!this.wrapperWidget._started){
this.wrapperWidget.startup();
}
if(!this._started){
this.startup();
}
}
var ww=this.wrapperWidget;
_5.add(this.displayNode,"dijitOffScreen");
_5.remove(ww.domNode,"dijitOffScreen");
_7.set(ww.domNode,{visibility:"visible"});
_4.set(this.displayNode,"tabIndex","-1");
var ew=ww.editWidget;
var _28=this;
_e(ew.onLoadDeferred,_c.hitch(ww,function(){
ew.set(("displayedValue" in ew||"_setDisplayedValueAttr" in ew)?"displayedValue":"value",_28.value);
this.defer(function(){
ww.saveButton.set("disabled","intermediateChanges" in ew);
this.focus();
this._resetValue=this.getValue();
});
}));
},_onBlur:function(){
this.inherited(arguments);
if(!this.editing){
}
},destroy:function(){
if(this.wrapperWidget&&!this.wrapperWidget._destroyed){
this.wrapperWidget.destroy();
delete this.wrapperWidget;
}
this.inherited(arguments);
},_showText:function(_29){
var ww=this.wrapperWidget;
_7.set(ww.domNode,{visibility:"hidden"});
_5.add(ww.domNode,"dijitOffScreen");
_5.remove(this.displayNode,"dijitOffScreen");
_4.set(this.displayNode,"tabIndex",this._savedTabIndex);
if(_29){
fm.focus(this.displayNode);
}
},save:function(_2a){
if(this.disabled||!this.editing){
return;
}
this._set("editing",false);
var ww=this.wrapperWidget;
var _2b=ww.getValue();
this.set("value",_2b);
this._showText(_2a);
},setValue:function(val){
_a.deprecated("dijit.InlineEditBox.setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
return this.set("value",val);
},_setValueAttr:function(val){
val=_c.trim(val);
var _2c=this.renderAsHtml?val:val.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;").replace(/\n/g,"<br>");
if(this.editorParams&&this.editorParams.type==="password"){
this.displayNode.innerHTML="********";
}else{
this.displayNode.innerHTML=_2c||this.noValueIndicator;
}
this._set("value",val);
if(this._started){
this.defer(function(){
this.onChange(val);
});
}
if(this.textDir=="auto"){
this.applyTextDir(this.displayNode,this.displayNode.innerText);
}
},getValue:function(){
_a.deprecated("dijit.InlineEditBox.getValue() is deprecated.  Use get('value') instead.","","2.0");
return this.get("value");
},cancel:function(_2d){
if(this.disabled||!this.editing){
return;
}
this._set("editing",false);
this.defer("onCancel");
this._showText(_2d);
},_setTextDirAttr:function(_2e){
if(!this._created||this.textDir!=_2e){
this._set("textDir",_2e);
this.applyTextDir(this.displayNode,this.displayNode.innerText);
this.displayNode.align=this.dir=="rtl"?"right":"left";
}
}});
_22._InlineEditor=_17;
return _22;
});
