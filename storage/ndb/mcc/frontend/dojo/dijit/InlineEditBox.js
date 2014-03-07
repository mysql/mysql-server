//>>built
require({cache:{"url:dijit/templates/InlineEditBox.html":"<span data-dojo-attach-point=\"editNode\" role=\"presentation\" style=\"position: absolute; visibility:hidden\" class=\"dijitReset dijitInline\"\n\tdata-dojo-attach-event=\"onkeypress: _onKeyPress\"\n\t><span data-dojo-attach-point=\"editorPlaceholder\"></span\n\t><span data-dojo-attach-point=\"buttonContainer\"\n\t\t><button data-dojo-type=\"dijit.form.Button\" data-dojo-props=\"label: '${buttonSave}', 'class': 'saveButton'\"\n\t\t\tdata-dojo-attach-point=\"saveButton\" data-dojo-attach-event=\"onClick:save\"></button\n\t\t><button data-dojo-type=\"dijit.form.Button\"  data-dojo-props=\"label: '${buttonCancel}', 'class': 'cancelButton'\"\n\t\t\tdata-dojo-attach-point=\"cancelButton\" data-dojo-attach-event=\"onClick:cancel\"></button\n\t></span\n></span>\n"}});
define("dijit/InlineEditBox",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/_base/event","dojo/i18n","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/_base/sniff","./focus","./_Widget","./_TemplatedMixin","./_WidgetsInTemplateMixin","./_Container","./form/Button","./form/_TextBoxMixin","./form/TextBox","dojo/text!./templates/InlineEditBox.html","dojo/i18n!./nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,fm,_d,_e,_f,_10,_11,_12,_13,_14){
var _15=_2("dijit._InlineEditor",[_d,_e,_f],{templateString:_14,postMixInProperties:function(){
this.inherited(arguments);
this.messages=_8.getLocalization("dijit","common",this.lang);
_1.forEach(["buttonSave","buttonCancel"],function(_16){
if(!this[_16]){
this[_16]=this.messages[_16];
}
},this);
},buildRendering:function(){
this.inherited(arguments);
var cls=typeof this.editor=="string"?_b.getObject(this.editor):this.editor;
var _17=this.sourceStyle,_18="line-height:"+_17.lineHeight+";",_19=_6.getComputedStyle(this.domNode);
_1.forEach(["Weight","Family","Size","Style"],function(_1a){
var _1b=_17["font"+_1a],_1c=_19["font"+_1a];
if(_1c!=_1b){
_18+="font-"+_1a+":"+_17["font"+_1a]+";";
}
},this);
_1.forEach(["marginTop","marginBottom","marginLeft","marginRight"],function(_1d){
this.domNode.style[_1d]=_17[_1d];
},this);
var _1e=this.inlineEditBox.width;
if(_1e=="100%"){
_18+="width:100%;";
this.domNode.style.display="block";
}else{
_18+="width:"+(_1e+(Number(_1e)==_1e?"px":""))+";";
}
var _1f=_b.delegate(this.inlineEditBox.editorParams,{style:_18,dir:this.dir,lang:this.lang,textDir:this.textDir});
_1f["displayedValue" in cls.prototype?"displayedValue":"value"]=this.value;
this.editWidget=new cls(_1f,this.editorPlaceholder);
if(this.inlineEditBox.autoSave){
_5.destroy(this.buttonContainer);
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
},_onIntermediateChange:function(){
this.saveButton.set("disabled",(this.getValue()==this._resetValue)||!this.enableSave());
},destroy:function(){
this.editWidget.destroy(true);
this.inherited(arguments);
},getValue:function(){
var ew=this.editWidget;
return String(ew.get("displayedValue" in ew?"displayedValue":"value"));
},_onKeyPress:function(e){
if(this.inlineEditBox.autoSave&&this.inlineEditBox.editing){
if(e.altKey||e.ctrlKey){
return;
}
if(e.charOrCode==_a.ESCAPE){
_7.stop(e);
this.cancel(true);
}else{
if(e.charOrCode==_a.ENTER&&e.target.tagName=="INPUT"){
_7.stop(e);
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
return (this.editWidget.isValid?this.editWidget.isValid():true);
},focus:function(){
this.editWidget.focus();
setTimeout(_b.hitch(this,function(){
if(this.editWidget.focusNode&&this.editWidget.focusNode.tagName=="INPUT"){
_12.selectInputText(this.editWidget.focusNode);
}
}),0);
}});
var _20=_2("dijit.InlineEditBox",_d,{editing:false,autoSave:true,buttonSave:"",buttonCancel:"",renderAsHtml:false,editor:_13,editorWrapper:_15,editorParams:{},disabled:false,onChange:function(){
},onCancel:function(){
},width:"100%",value:"",noValueIndicator:_c("ie")<=6?"<span style='font-family: wingdings; text-decoration: underline;'>&#160;&#160;&#160;&#160;&#x270d;&#160;&#160;&#160;&#160;</span>":"<span style='text-decoration: underline;'>&#160;&#160;&#160;&#160;&#x270d;&#160;&#160;&#160;&#160;</span>",constructor:function(){
this.editorParams={};
},postMixInProperties:function(){
this.inherited(arguments);
this.displayNode=this.srcNodeRef;
var _21={ondijitclick:"_onClick",onmouseover:"_onMouseOver",onmouseout:"_onMouseOut",onfocus:"_onMouseOver",onblur:"_onMouseOut"};
for(var _22 in _21){
this.connect(this.displayNode,_22,_21[_22]);
}
this.displayNode.setAttribute("role","button");
if(!this.displayNode.getAttribute("tabIndex")){
this.displayNode.setAttribute("tabIndex",0);
}
if(!this.value&&!("value" in this.params)){
this.value=_b.trim(this.renderAsHtml?this.displayNode.innerHTML:(this.displayNode.innerText||this.displayNode.textContent||""));
}
if(!this.value){
this.displayNode.innerHTML=this.noValueIndicator;
}
_4.add(this.displayNode,"dijitInlineEditBoxDisplayMode");
},setDisabled:function(_23){
_9.deprecated("dijit.InlineEditBox.setDisabled() is deprecated.  Use set('disabled', bool) instead.","","2.0");
this.set("disabled",_23);
},_setDisabledAttr:function(_24){
this.domNode.setAttribute("aria-disabled",_24);
if(_24){
this.displayNode.removeAttribute("tabIndex");
}else{
this.displayNode.setAttribute("tabIndex",0);
}
_4.toggle(this.displayNode,"dijitInlineEditBoxDisplayModeDisabled",_24);
this._set("disabled",_24);
},_onMouseOver:function(){
if(!this.disabled){
_4.add(this.displayNode,"dijitInlineEditBoxDisplayModeHover");
}
},_onMouseOut:function(){
_4.remove(this.displayNode,"dijitInlineEditBoxDisplayModeHover");
},_onClick:function(e){
if(this.disabled){
return;
}
if(e){
_7.stop(e);
}
this._onMouseOut();
setTimeout(_b.hitch(this,"edit"),0);
},edit:function(){
if(this.disabled||this.editing){
return;
}
this._set("editing",true);
this._savedPosition=_6.get(this.displayNode,"position")||"static";
this._savedOpacity=_6.get(this.displayNode,"opacity")||"1";
this._savedTabIndex=_3.get(this.displayNode,"tabIndex")||"0";
if(this.wrapperWidget){
var ew=this.wrapperWidget.editWidget;
ew.set("displayedValue" in ew?"displayedValue":"value",this.value);
}else{
var _25=_5.create("span",null,this.domNode,"before");
var ewc=typeof this.editorWrapper=="string"?_b.getObject(this.editorWrapper):this.editorWrapper;
this.wrapperWidget=new ewc({value:this.value,buttonSave:this.buttonSave,buttonCancel:this.buttonCancel,dir:this.dir,lang:this.lang,tabIndex:this._savedTabIndex,editor:this.editor,inlineEditBox:this,sourceStyle:_6.getComputedStyle(this.displayNode),save:_b.hitch(this,"save"),cancel:_b.hitch(this,"cancel"),textDir:this.textDir},_25);
if(!this._started){
this.startup();
}
}
var ww=this.wrapperWidget;
_6.set(this.displayNode,{position:"absolute",opacity:"0"});
_6.set(ww.domNode,{position:this._savedPosition,visibility:"visible",opacity:"1"});
_3.set(this.displayNode,"tabIndex","-1");
setTimeout(_b.hitch(ww,function(){
this.focus();
this._resetValue=this.getValue();
}),0);
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
},_showText:function(_26){
var ww=this.wrapperWidget;
_6.set(ww.domNode,{position:"absolute",visibility:"hidden",opacity:"0"});
_6.set(this.displayNode,{position:this._savedPosition,opacity:this._savedOpacity});
_3.set(this.displayNode,"tabIndex",this._savedTabIndex);
if(_26){
fm.focus(this.displayNode);
}
},save:function(_27){
if(this.disabled||!this.editing){
return;
}
this._set("editing",false);
var ww=this.wrapperWidget;
var _28=ww.getValue();
this.set("value",_28);
this._showText(_27);
},setValue:function(val){
_9.deprecated("dijit.InlineEditBox.setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
return this.set("value",val);
},_setValueAttr:function(val){
val=_b.trim(val);
var _29=this.renderAsHtml?val:val.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;").replace(/\n/g,"<br>");
this.displayNode.innerHTML=_29||this.noValueIndicator;
this._set("value",val);
if(this._started){
setTimeout(_b.hitch(this,"onChange",val),0);
}
if(this.textDir=="auto"){
this.applyTextDir(this.displayNode,this.displayNode.innerText);
}
},getValue:function(){
_9.deprecated("dijit.InlineEditBox.getValue() is deprecated.  Use get('value') instead.","","2.0");
return this.get("value");
},cancel:function(_2a){
if(this.disabled||!this.editing){
return;
}
this._set("editing",false);
setTimeout(_b.hitch(this,"onCancel"),0);
this._showText(_2a);
},_setTextDirAttr:function(_2b){
if(!this._created||this.textDir!=_2b){
this._set("textDir",_2b);
this.applyTextDir(this.displayNode,this.displayNode.innerText);
this.displayNode.align=this.dir=="rtl"?"right":"left";
}
}});
_20._InlineEditor=_15;
return _20;
});
