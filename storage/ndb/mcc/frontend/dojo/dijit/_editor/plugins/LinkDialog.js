//>>built
define("dijit/_editor/plugins/LinkDialog",["require","dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/query","dojo/string","../_Plugin","../../form/DropDownButton","../range"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9,_a,_b,_c){
var _d=_3("dijit._editor.plugins.LinkDialog",_a,{allowUnsafeHtml:false,linkFilter:[[/</g,"&lt;"]],buttonClass:_b,useDefaultCommand:false,urlRegExp:"((https?|ftps?|file)\\://|./|../|/|)(/[a-zA-Z]{1,1}:/|)(((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)*(?:[a-zA-Z](?:[-\\da-zA-Z]{0,80}[\\da-zA-Z])?)\\.?)|(((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])|(0[xX]0*[\\da-fA-F]?[\\da-fA-F]\\.){3}0[xX]0*[\\da-fA-F]?[\\da-fA-F]|(0+[0-3][0-7][0-7]\\.){3}0+[0-3][0-7][0-7]|(0|[1-9]\\d{0,8}|[1-3]\\d{9}|4[01]\\d{8}|42[0-8]\\d{7}|429[0-3]\\d{6}|4294[0-8]\\d{5}|42949[0-5]\\d{4}|429496[0-6]\\d{3}|4294967[01]\\d{2}|42949672[0-8]\\d|429496729[0-5])|0[xX]0*[\\da-fA-F]{1,8}|([\\da-fA-F]{1,4}\\:){7}[\\da-fA-F]{1,4}|([\\da-fA-F]{1,4}\\:){6}((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])))(\\:\\d+)?(/(?:[^?#\\s/]+/)*(?:[^?#\\s/]{0,}(?:\\?[^?#\\s/]*)?(?:#.*)?)?)?",emailRegExp:"<?(mailto\\:)([!#-'*+\\-\\/-9=?A-Z^-~]+[.])*[!#-'*+\\-\\/-9=?A-Z^-~]+"+"@"+"((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)+(?:[a-zA-Z](?:[-\\da-zA-Z]{0,6}[\\da-zA-Z])?)\\.?)|localhost|^[^-][a-zA-Z0-9_-]*>?",htmlTemplate:"<a href=\"${urlInput}\" _djrealurl=\"${urlInput}\""+" target=\"${targetSelect}\""+">${textInput}</a>",tag:"a",_hostRxp:/^((([^\[:]+):)?([^@]+)@)?(\[([^\]]+)\]|([^\[:]*))(:([0-9]+))?$/,_userAtRxp:/^([!#-'*+\-\/-9=?A-Z^-~]+[.])*[!#-'*+\-\/-9=?A-Z^-~]+@/i,linkDialogTemplate:["<table role='presentation'><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' "+"id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_targetSelect'>${target}</label>","</td><td>","<select id='${id}_targetSelect' name='targetSelect' data-dojo-type='dijit.form.Select'>","<option selected='selected' value='_self'>${currentWindow}</option>","<option value='_blank'>${newWindow}</option>","<option value='_top'>${topWindow}</option>","<option value='_parent'>${parentWindow}</option>","</select>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),_initButton:function(){
this.inherited(arguments);
this.button.loadDropDown=_6.hitch(this,"_loadDropDown");
this._connectTagEvents();
},_loadDropDown:function(_e){
_1(["dojo/i18n","../../TooltipDialog","../../registry","../../form/Button","../../form/Select","../../form/ValidationTextBox","dojo/i18n!../../nls/common","dojo/i18n!../nls/LinkDialog"],_6.hitch(this,function(_f,_10,_11){
var _12=this;
this.tag=this.command=="insertImage"?"img":"a";
var _13=_6.delegate(_f.getLocalization("dijit","common",this.lang),_f.getLocalization("dijit._editor","LinkDialog",this.lang));
var _14=(this.dropDown=this.button.dropDown=new _10({title:_13[this.command+"Title"],ownerDocument:this.editor.ownerDocument,dir:this.editor.dir,execute:_6.hitch(this,"setValue"),onOpen:function(){
_12._onOpenDialog();
_10.prototype.onOpen.apply(this,arguments);
},onCancel:function(){
setTimeout(_6.hitch(_12,"_onCloseDialog"),0);
}}));
_13.urlRegExp=this.urlRegExp;
_13.id=_11.getUniqueId(this.editor.id);
this._uniqueId=_13.id;
this._setContent(_14.title+"<div style='border-bottom: 1px black solid;padding-bottom:2pt;margin-bottom:4pt'></div>"+_9.substitute(this.linkDialogTemplate,_13));
_14.startup();
this._urlInput=_11.byId(this._uniqueId+"_urlInput");
this._textInput=_11.byId(this._uniqueId+"_textInput");
this._setButton=_11.byId(this._uniqueId+"_setButton");
this.own(_11.byId(this._uniqueId+"_cancelButton").on("click",_6.hitch(this.dropDown,"onCancel")));
if(this._urlInput){
this.own(this._urlInput.on("change",_6.hitch(this,"_checkAndFixInput")));
}
if(this._textInput){
this.own(this._textInput.on("change",_6.hitch(this,"_checkAndFixInput")));
}
this._urlRegExp=new RegExp("^"+this.urlRegExp+"$","i");
this._emailRegExp=new RegExp("^"+this.emailRegExp+"$","i");
this._urlInput.isValid=_6.hitch(this,function(){
var _15=this._urlInput.get("value");
return this._urlRegExp.test(_15)||this._emailRegExp.test(_15);
});
this.own(on(_14.domNode,"keydown",_6.hitch(this,_6.hitch(this,function(e){
if(e&&e.keyCode==_5.ENTER&&!e.shiftKey&&!e.metaKey&&!e.ctrlKey&&!e.altKey){
if(!this._setButton.get("disabled")){
_14.onExecute();
_14.execute(_14.get("value"));
}
}
}))));
_e();
}));
},_checkAndFixInput:function(){
var _16=this;
var url=this._urlInput.get("value");
var _17=function(url){
var _18=false;
var _19=false;
if(url&&url.length>1){
url=_6.trim(url);
if(url.indexOf("mailto:")!==0){
if(url.indexOf("/")>0){
if(url.indexOf("://")===-1){
if(url.charAt(0)!=="/"&&url.indexOf("./")&&url.indexOf("../")!==0){
if(_16._hostRxp.test(url)){
_18=true;
}
}
}
}else{
if(_16._userAtRxp.test(url)){
_19=true;
}
}
}
}
if(_18){
_16._urlInput.set("value","http://"+url);
}
if(_19){
_16._urlInput.set("value","mailto:"+url);
}
_16._setButton.set("disabled",!_16._isValid());
};
if(this._delayedCheck){
clearTimeout(this._delayedCheck);
this._delayedCheck=null;
}
this._delayedCheck=setTimeout(function(){
_17(url);
},250);
},_connectTagEvents:function(){
this.editor.onLoadDeferred.then(_6.hitch(this,function(){
this.own(on(this.editor.editNode,"mouseup",_6.hitch(this,"_onMouseUp")));
this.own(on(this.editor.editNode,"dblclick",_6.hitch(this,"_onDblClick")));
}));
},_isValid:function(){
return this._urlInput.isValid()&&this._textInput.isValid();
},_setContent:function(_1a){
this.dropDown.set({parserScope:"dojo",content:_1a});
},_checkValues:function(_1b){
if(_1b&&_1b.urlInput){
_1b.urlInput=_1b.urlInput.replace(/"/g,"&quot;");
}
if(!this.allowUnsafeHtml&&_1b&&_1b.textInput){
if(typeof this.linkFilter==="function"){
_1b.textInput=this.linkFilter(_1b.textInput);
}else{
_2.forEach(this.linkFilter,function(_1c){
_1b.textInput=_1b.textInput.replace(_1c[0],_1c[1]);
});
}
}
return _1b;
},_createlinkEnabledImpl:function(){
return true;
},setValue:function(_1d){
this._onCloseDialog();
if(_7("ie")<9){
var sel=_c.getSelection(this.editor.window);
var _1e=sel.getRangeAt(0);
var a=_1e.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!==this.tag)){
a=this.editor.selection.getSelectedElement(this.tag);
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()===this.tag)){
if(this.editor.queryCommandEnabled("unlink")){
this.editor.selection.selectElementChildren(a);
this.editor.execCommand("unlink");
}
}
}
_1d=this._checkValues(_1d);
this.editor.execCommand("inserthtml",_9.substitute(this.htmlTemplate,_1d));
_8("a",this.editor.document).forEach(function(a){
if(!a.innerHTML&&!_4.has(a,"name")){
a.parentNode.removeChild(a);
}
},this);
},_onCloseDialog:function(){
if(this.editor.focused){
this.editor.focus();
}
},_getCurrentValues:function(a){
var url,_1f,_20;
if(a&&a.tagName.toLowerCase()===this.tag){
url=a.getAttribute("_djrealurl")||a.getAttribute("href");
_20=a.getAttribute("target")||"_self";
_1f=a.textContent||a.innerText;
this.editor.selection.selectElement(a,true);
}else{
_1f=this.editor.selection.getSelectedText();
}
return {urlInput:url||"",textInput:_1f||"",targetSelect:_20||""};
},_onOpenDialog:function(){
var a,b,fc;
if(_7("ie")){
var sel=_c.getSelection(this.editor.window);
if(sel.rangeCount){
var _21=sel.getRangeAt(0);
a=_21.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!==this.tag)){
a=this.editor.selection.getSelectedElement(this.tag);
}
if(!a||(a.nodeName&&a.nodeName.toLowerCase()!==this.tag)){
b=this.editor.selection.getAncestorElement(this.tag);
if(b&&(b.nodeName&&b.nodeName.toLowerCase()==this.tag)){
a=b;
this.editor.selection.selectElement(a);
}else{
if(_21.startContainer===_21.endContainer){
fc=_21.startContainer.firstChild;
if(fc&&(fc.nodeName&&fc.nodeName.toLowerCase()==this.tag)){
a=fc;
this.editor.selection.selectElement(a);
}
}
}
}
}
}else{
a=this.editor.selection.getAncestorElement(this.tag);
}
this.dropDown.reset();
this._setButton.set("disabled",true);
this.dropDown.set("value",this._getCurrentValues(a));
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag&&_4.get(t,"href")){
var _22=this.editor;
this.editor.selection.selectElement(t);
_22.onDisplayChanged();
if(_22._updateTimer){
_22._updateTimer.remove();
delete _22._updateTimer;
}
_22.onNormalizedDisplayChanged();
var _23=this.button;
setTimeout(function(){
_23.set("disabled",false);
_23.loadAndOpenDropDown().then(function(){
if(_23.dropDown.focus){
_23.dropDown.focus();
}
});
},10);
}
}
},_onMouseUp:function(){
if(_7("ff")){
var a=this.editor.selection.getAncestorElement(this.tag);
if(a){
var _24=_c.getSelection(this.editor.window);
var _25=_24.getRangeAt(0);
if(_25.collapsed&&a.childNodes.length){
var _26=_25.cloneRange();
_26.selectNodeContents(a.childNodes[a.childNodes.length-1]);
_26.setStart(a.childNodes[0],0);
if(_25.compareBoundaryPoints(_26.START_TO_START,_26)!==1){
_25.setStartBefore(a);
}else{
if(_25.compareBoundaryPoints(_26.END_TO_START,_26)!==-1){
_25.setStartAfter(a);
}
}
}
}
}
}});
var _27=_3("dijit._editor.plugins.ImgLinkDialog",[_d],{linkDialogTemplate:["<table role='presentation'><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' regExp='${urlRegExp}' "+"required='true' id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='false' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","</td><td>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),htmlTemplate:"<img src=\"${urlInput}\" _djrealurl=\"${urlInput}\" alt=\"${textInput}\" />",tag:"img",_getCurrentValues:function(img){
var url,_28;
if(img&&img.tagName.toLowerCase()===this.tag){
url=img.getAttribute("_djrealurl")||img.getAttribute("src");
_28=img.getAttribute("alt");
this.editor.selection.selectElement(img,true);
}else{
_28=this.editor.selection.getSelectedText();
}
return {urlInput:url||"",textInput:_28||""};
},_isValid:function(){
return this._urlInput.isValid();
},_connectTagEvents:function(){
this.inherited(arguments);
this.editor.onLoadDeferred.then(_6.hitch(this,function(){
this.own(on(this.editor.editNode,"mousedown",_6.hitch(this,"_selectTag")));
}));
},_selectTag:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag){
this.editor.selection.selectElement(t);
}
}
},_checkValues:function(_29){
if(_29&&_29.urlInput){
_29.urlInput=_29.urlInput.replace(/"/g,"&quot;");
}
if(_29&&_29.textInput){
_29.textInput=_29.textInput.replace(/"/g,"&quot;");
}
return _29;
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag&&_4.get(t,"src")){
var _2a=this.editor;
this.editor.selection.selectElement(t);
_2a.onDisplayChanged();
if(_2a._updateTimer){
_2a._updateTimer.remove();
delete _2a._updateTimer;
}
_2a.onNormalizedDisplayChanged();
var _2b=this.button;
setTimeout(function(){
_2b.set("disabled",false);
_2b.loadAndOpenDropDown().then(function(){
if(_2b.dropDown.focus){
_2b.dropDown.focus();
}
});
},10);
}
}
}});
_a.registry["createLink"]=function(_2c){
var _2d={command:"createLink",allowUnsafeHtml:("allowUnsafeHtml" in _2c)?_2c.allowUnsafeHtml:false};
if("linkFilter" in _2c){
_2d.linkFilter=_2c.linkFilter;
}
return new _d(_2d);
};
_a.registry["insertImage"]=function(){
return new _27({command:"insertImage"});
};
_d.ImgLinkDialog=_27;
return _d;
});
