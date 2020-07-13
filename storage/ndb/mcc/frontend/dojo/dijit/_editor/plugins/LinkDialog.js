//>>built
define("dijit/_editor/plugins/LinkDialog",["require","dojo/_base/declare","dojo/dom-attr","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/query","dojo/string","../_Plugin","../../form/DropDownButton","../range"],function(_1,_2,_3,_4,_5,on,_6,_7,_8,_9,_a,_b){
var _c=_2("dijit._editor.plugins.LinkDialog",_9,{buttonClass:_a,useDefaultCommand:false,urlRegExp:"((https?|ftps?|file)\\://|./|../|/|)(/[a-zA-Z]{1,1}:/|)(((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)*(?:[a-zA-Z](?:[-\\da-zA-Z]{0,80}[\\da-zA-Z])?)\\.?)|(((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])|(0[xX]0*[\\da-fA-F]?[\\da-fA-F]\\.){3}0[xX]0*[\\da-fA-F]?[\\da-fA-F]|(0+[0-3][0-7][0-7]\\.){3}0+[0-3][0-7][0-7]|(0|[1-9]\\d{0,8}|[1-3]\\d{9}|4[01]\\d{8}|42[0-8]\\d{7}|429[0-3]\\d{6}|4294[0-8]\\d{5}|42949[0-5]\\d{4}|429496[0-6]\\d{3}|4294967[01]\\d{2}|42949672[0-8]\\d|429496729[0-5])|0[xX]0*[\\da-fA-F]{1,8}|([\\da-fA-F]{1,4}\\:){7}[\\da-fA-F]{1,4}|([\\da-fA-F]{1,4}\\:){6}((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])))(\\:\\d+)?(/(?:[^?#\\s/]+/)*(?:[^?#\\s/]{0,}(?:\\?[^?#\\s/]*)?(?:#.*)?)?)?",emailRegExp:"<?(mailto\\:)([!#-'*+\\-\\/-9=?A-Z^-~]+[.])*[!#-'*+\\-\\/-9=?A-Z^-~]+"+"@"+"((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)+(?:[a-zA-Z](?:[-\\da-zA-Z]{0,6}[\\da-zA-Z])?)\\.?)|localhost|^[^-][a-zA-Z0-9_-]*>?",htmlTemplate:"<a href=\"${urlInput}\" _djrealurl=\"${urlInput}\""+" target=\"${targetSelect}\""+">${textInput}</a>",tag:"a",_hostRxp:/^((([^\[:]+):)?([^@]+)@)?(\[([^\]]+)\]|([^\[:]*))(:([0-9]+))?$/,_userAtRxp:/^([!#-'*+\-\/-9=?A-Z^-~]+[.])*[!#-'*+\-\/-9=?A-Z^-~]+@/i,linkDialogTemplate:["<table role='presentation'><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' "+"id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_targetSelect'>${target}</label>","</td><td>","<select id='${id}_targetSelect' name='targetSelect' data-dojo-type='dijit.form.Select'>","<option selected='selected' value='_self'>${currentWindow}</option>","<option value='_blank'>${newWindow}</option>","<option value='_top'>${topWindow}</option>","<option value='_parent'>${parentWindow}</option>","</select>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),_initButton:function(){
this.inherited(arguments);
this.button.loadDropDown=_5.hitch(this,"_loadDropDown");
this._connectTagEvents();
},_loadDropDown:function(_d){
_1(["dojo/i18n","../../TooltipDialog","../../registry","../../form/Button","../../form/Select","../../form/ValidationTextBox","dojo/i18n!../../nls/common","dojo/i18n!../nls/LinkDialog"],_5.hitch(this,function(_e,_f,_10){
var _11=this;
this.tag=this.command=="insertImage"?"img":"a";
var _12=_5.delegate(_e.getLocalization("dijit","common",this.lang),_e.getLocalization("dijit._editor","LinkDialog",this.lang));
var _13=(this.dropDown=this.button.dropDown=new _f({title:_12[this.command+"Title"],ownerDocument:this.editor.ownerDocument,dir:this.editor.dir,execute:_5.hitch(this,"setValue"),onOpen:function(){
_11._onOpenDialog();
_f.prototype.onOpen.apply(this,arguments);
},onCancel:function(){
setTimeout(_5.hitch(_11,"_onCloseDialog"),0);
}}));
_12.urlRegExp=this.urlRegExp;
_12.id=_10.getUniqueId(this.editor.id);
this._uniqueId=_12.id;
this._setContent(_13.title+"<div style='border-bottom: 1px black solid;padding-bottom:2pt;margin-bottom:4pt'></div>"+_8.substitute(this.linkDialogTemplate,_12));
_13.startup();
this._urlInput=_10.byId(this._uniqueId+"_urlInput");
this._textInput=_10.byId(this._uniqueId+"_textInput");
this._setButton=_10.byId(this._uniqueId+"_setButton");
this.own(_10.byId(this._uniqueId+"_cancelButton").on("click",_5.hitch(this.dropDown,"onCancel")));
if(this._urlInput){
this.own(this._urlInput.on("change",_5.hitch(this,"_checkAndFixInput")));
}
if(this._textInput){
this.own(this._textInput.on("change",_5.hitch(this,"_checkAndFixInput")));
}
this._urlRegExp=new RegExp("^"+this.urlRegExp+"$","i");
this._emailRegExp=new RegExp("^"+this.emailRegExp+"$","i");
this._urlInput.isValid=_5.hitch(this,function(){
var _14=this._urlInput.get("value");
return this._urlRegExp.test(_14)||this._emailRegExp.test(_14);
});
this.own(on(_13.domNode,"keydown",_5.hitch(this,_5.hitch(this,function(e){
if(e&&e.keyCode==_4.ENTER&&!e.shiftKey&&!e.metaKey&&!e.ctrlKey&&!e.altKey){
if(!this._setButton.get("disabled")){
_13.onExecute();
_13.execute(_13.get("value"));
}
}
}))));
_d();
}));
},_checkAndFixInput:function(){
var _15=this;
var url=this._urlInput.get("value");
var _16=function(url){
var _17=false;
var _18=false;
if(url&&url.length>1){
url=_5.trim(url);
if(url.indexOf("mailto:")!==0){
if(url.indexOf("/")>0){
if(url.indexOf("://")===-1){
if(url.charAt(0)!=="/"&&url.indexOf("./")&&url.indexOf("../")!==0){
if(_15._hostRxp.test(url)){
_17=true;
}
}
}
}else{
if(_15._userAtRxp.test(url)){
_18=true;
}
}
}
}
if(_17){
_15._urlInput.set("value","http://"+url);
}
if(_18){
_15._urlInput.set("value","mailto:"+url);
}
_15._setButton.set("disabled",!_15._isValid());
};
if(this._delayedCheck){
clearTimeout(this._delayedCheck);
this._delayedCheck=null;
}
this._delayedCheck=setTimeout(function(){
_16(url);
},250);
},_connectTagEvents:function(){
this.editor.onLoadDeferred.then(_5.hitch(this,function(){
this.own(on(this.editor.editNode,"mouseup",_5.hitch(this,"_onMouseUp")));
this.own(on(this.editor.editNode,"dblclick",_5.hitch(this,"_onDblClick")));
}));
},_isValid:function(){
return this._urlInput.isValid()&&this._textInput.isValid();
},_setContent:function(_19){
this.dropDown.set({parserScope:"dojo",content:_19});
},_checkValues:function(_1a){
if(_1a&&_1a.urlInput){
_1a.urlInput=_1a.urlInput.replace(/"/g,"&quot;");
}
return _1a;
},_createlinkEnabledImpl:function(){
return true;
},setValue:function(_1b){
this._onCloseDialog();
if(_6("ie")<9){
var sel=_b.getSelection(this.editor.window);
var _1c=sel.getRangeAt(0);
var a=_1c.endContainer;
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
_1b=this._checkValues(_1b);
this.editor.execCommand("inserthtml",_8.substitute(this.htmlTemplate,_1b));
_7("a",this.editor.document).forEach(function(a){
if(!a.innerHTML&&!_3.has(a,"name")){
a.parentNode.removeChild(a);
}
},this);
},_onCloseDialog:function(){
if(this.editor.focused){
this.editor.focus();
}
},_getCurrentValues:function(a){
var url,_1d,_1e;
if(a&&a.tagName.toLowerCase()===this.tag){
url=a.getAttribute("_djrealurl")||a.getAttribute("href");
_1e=a.getAttribute("target")||"_self";
_1d=a.textContent||a.innerText;
this.editor.selection.selectElement(a,true);
}else{
_1d=this.editor.selection.getSelectedText();
}
return {urlInput:url||"",textInput:_1d||"",targetSelect:_1e||""};
},_onOpenDialog:function(){
var a,b,fc;
if(_6("ie")){
var sel=_b.getSelection(this.editor.window);
if(sel.rangeCount){
var _1f=sel.getRangeAt(0);
a=_1f.endContainer;
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
if(_1f.startContainer===_1f.endContainer){
fc=_1f.startContainer.firstChild;
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
if(tg===this.tag&&_3.get(t,"href")){
var _20=this.editor;
this.editor.selection.selectElement(t);
_20.onDisplayChanged();
if(_20._updateTimer){
_20._updateTimer.remove();
delete _20._updateTimer;
}
_20.onNormalizedDisplayChanged();
var _21=this.button;
setTimeout(function(){
_21.set("disabled",false);
_21.loadAndOpenDropDown().then(function(){
if(_21.dropDown.focus){
_21.dropDown.focus();
}
});
},10);
}
}
},_onMouseUp:function(){
if(_6("ff")){
var a=this.editor.selection.getAncestorElement(this.tag);
if(a){
var _22=_b.getSelection(this.editor.window);
var _23=_22.getRangeAt(0);
if(_23.collapsed&&a.childNodes.length){
var _24=_23.cloneRange();
_24.selectNodeContents(a.childNodes[a.childNodes.length-1]);
_24.setStart(a.childNodes[0],0);
if(_23.compareBoundaryPoints(_24.START_TO_START,_24)!==1){
_23.setStartBefore(a);
}else{
if(_23.compareBoundaryPoints(_24.END_TO_START,_24)!==-1){
_23.setStartAfter(a);
}
}
}
}
}
}});
var _25=_2("dijit._editor.plugins.ImgLinkDialog",[_c],{linkDialogTemplate:["<table role='presentation'><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' regExp='${urlRegExp}' "+"required='true' id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='false' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","</td><td>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),htmlTemplate:"<img src=\"${urlInput}\" _djrealurl=\"${urlInput}\" alt=\"${textInput}\" />",tag:"img",_getCurrentValues:function(img){
var url,_26;
if(img&&img.tagName.toLowerCase()===this.tag){
url=img.getAttribute("_djrealurl")||img.getAttribute("src");
_26=img.getAttribute("alt");
this.editor.selection.selectElement(img,true);
}else{
_26=this.editor.selection.getSelectedText();
}
return {urlInput:url||"",textInput:_26||""};
},_isValid:function(){
return this._urlInput.isValid();
},_connectTagEvents:function(){
this.inherited(arguments);
this.editor.onLoadDeferred.then(_5.hitch(this,function(){
this.own(on(this.editor.editNode,"mousedown",_5.hitch(this,"_selectTag")));
}));
},_selectTag:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag){
this.editor.selection.selectElement(t);
}
}
},_checkValues:function(_27){
if(_27&&_27.urlInput){
_27.urlInput=_27.urlInput.replace(/"/g,"&quot;");
}
if(_27&&_27.textInput){
_27.textInput=_27.textInput.replace(/"/g,"&quot;");
}
return _27;
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag&&_3.get(t,"src")){
var _28=this.editor;
this.editor.selection.selectElement(t);
_28.onDisplayChanged();
if(_28._updateTimer){
_28._updateTimer.remove();
delete _28._updateTimer;
}
_28.onNormalizedDisplayChanged();
var _29=this.button;
setTimeout(function(){
_29.set("disabled",false);
_29.loadAndOpenDropDown().then(function(){
if(_29.dropDown.focus){
_29.dropDown.focus();
}
});
},10);
}
}
}});
_9.registry["createLink"]=function(){
return new _c({command:"createLink"});
};
_9.registry["insertImage"]=function(){
return new _25({command:"insertImage"});
};
_c.ImgLinkDialog=_25;
return _c;
});
