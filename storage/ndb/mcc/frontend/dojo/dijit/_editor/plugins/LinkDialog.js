//>>built
define("dijit/_editor/plugins/LinkDialog",["require","dojo/_base/declare","dojo/dom-attr","dojo/keys","dojo/_base/lang","dojo/_base/sniff","dojo/string","dojo/_base/window","../../_Widget","../_Plugin","../../form/DropDownButton","../range","../selection"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_2("dijit._editor.plugins.LinkDialog",_a,{buttonClass:_b,useDefaultCommand:false,urlRegExp:"((https?|ftps?|file)\\://|./|/|)(/[a-zA-Z]{1,1}:/|)(((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)*(?:[a-zA-Z](?:[-\\da-zA-Z]{0,80}[\\da-zA-Z])?)\\.?)|(((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])|(0[xX]0*[\\da-fA-F]?[\\da-fA-F]\\.){3}0[xX]0*[\\da-fA-F]?[\\da-fA-F]|(0+[0-3][0-7][0-7]\\.){3}0+[0-3][0-7][0-7]|(0|[1-9]\\d{0,8}|[1-3]\\d{9}|4[01]\\d{8}|42[0-8]\\d{7}|429[0-3]\\d{6}|4294[0-8]\\d{5}|42949[0-5]\\d{4}|429496[0-6]\\d{3}|4294967[01]\\d{2}|42949672[0-8]\\d|429496729[0-5])|0[xX]0*[\\da-fA-F]{1,8}|([\\da-fA-F]{1,4}\\:){7}[\\da-fA-F]{1,4}|([\\da-fA-F]{1,4}\\:){6}((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])))(\\:\\d+)?(/(?:[^?#\\s/]+/)*(?:[^?#\\s/]{0,}(?:\\?[^?#\\s/]*)?(?:#.*)?)?)?",emailRegExp:"<?(mailto\\:)([!#-'*+\\-\\/-9=?A-Z^-~]+[.])*[!#-'*+\\-\\/-9=?A-Z^-~]+"+"@"+"((?:(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)\\.)+(?:[a-zA-Z](?:[-\\da-zA-Z]{0,6}[\\da-zA-Z])?)\\.?)|localhost|^[^-][a-zA-Z0-9_-]*>?",htmlTemplate:"<a href=\"${urlInput}\" _djrealurl=\"${urlInput}\""+" target=\"${targetSelect}\""+">${textInput}</a>",tag:"a",_hostRxp:/^((([^\[:]+):)?([^@]+)@)?(\[([^\]]+)\]|([^\[:]*))(:([0-9]+))?$/,_userAtRxp:/^([!#-'*+\-\/-9=?A-Z^-~]+[.])*[!#-'*+\-\/-9=?A-Z^-~]+@/i,linkDialogTemplate:["<table><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' "+"id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='true' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_targetSelect'>${target}</label>","</td><td>","<select id='${id}_targetSelect' name='targetSelect' data-dojo-type='dijit.form.Select'>","<option selected='selected' value='_self'>${currentWindow}</option>","<option value='_blank'>${newWindow}</option>","<option value='_top'>${topWindow}</option>","<option value='_parent'>${parentWindow}</option>","</select>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),_initButton:function(){
this.inherited(arguments);
this.button.loadDropDown=_5.hitch(this,"_loadDropDown");
this._connectTagEvents();
},_loadDropDown:function(_f){
_1(["dojo/i18n","../../TooltipDialog","../../registry","../../form/Button","../../form/Select","../../form/ValidationTextBox","dojo/i18n!../../nls/common","dojo/i18n!../nls/LinkDialog"],_5.hitch(this,function(_10,_11,_12){
var _13=this;
this.tag=this.command=="insertImage"?"img":"a";
var _14=_5.delegate(_10.getLocalization("dijit","common",this.lang),_10.getLocalization("dijit._editor","LinkDialog",this.lang));
var _15=(this.dropDown=this.button.dropDown=new _11({title:_14[this.command+"Title"],execute:_5.hitch(this,"setValue"),onOpen:function(){
_13._onOpenDialog();
_11.prototype.onOpen.apply(this,arguments);
},onCancel:function(){
setTimeout(_5.hitch(_13,"_onCloseDialog"),0);
}}));
_14.urlRegExp=this.urlRegExp;
_14.id=_12.getUniqueId(this.editor.id);
this._uniqueId=_14.id;
this._setContent(_15.title+"<div style='border-bottom: 1px black solid;padding-bottom:2pt;margin-bottom:4pt'></div>"+_7.substitute(this.linkDialogTemplate,_14));
_15.startup();
this._urlInput=_12.byId(this._uniqueId+"_urlInput");
this._textInput=_12.byId(this._uniqueId+"_textInput");
this._setButton=_12.byId(this._uniqueId+"_setButton");
this.connect(_12.byId(this._uniqueId+"_cancelButton"),"onClick",function(){
this.dropDown.onCancel();
});
if(this._urlInput){
this.connect(this._urlInput,"onChange","_checkAndFixInput");
}
if(this._textInput){
this.connect(this._textInput,"onChange","_checkAndFixInput");
}
this._urlRegExp=new RegExp("^"+this.urlRegExp+"$","i");
this._emailRegExp=new RegExp("^"+this.emailRegExp+"$","i");
this._urlInput.isValid=_5.hitch(this,function(){
var _16=this._urlInput.get("value");
return this._urlRegExp.test(_16)||this._emailRegExp.test(_16);
});
this.connect(_15.domNode,"onkeypress",function(e){
if(e&&e.charOrCode==_4.ENTER&&!e.shiftKey&&!e.metaKey&&!e.ctrlKey&&!e.altKey){
if(!this._setButton.get("disabled")){
_15.onExecute();
_15.execute(_15.get("value"));
}
}
});
_f();
}));
},_checkAndFixInput:function(){
var _17=this;
var url=this._urlInput.get("value");
var _18=function(url){
var _19=false;
var _1a=false;
if(url&&url.length>1){
url=_5.trim(url);
if(url.indexOf("mailto:")!==0){
if(url.indexOf("/")>0){
if(url.indexOf("://")===-1){
if(url.charAt(0)!=="/"&&url.indexOf("./")!==0){
if(_17._hostRxp.test(url)){
_19=true;
}
}
}
}else{
if(_17._userAtRxp.test(url)){
_1a=true;
}
}
}
}
if(_19){
_17._urlInput.set("value","http://"+url);
}
if(_1a){
_17._urlInput.set("value","mailto:"+url);
}
_17._setButton.set("disabled",!_17._isValid());
};
if(this._delayedCheck){
clearTimeout(this._delayedCheck);
this._delayedCheck=null;
}
this._delayedCheck=setTimeout(function(){
_18(url);
},250);
},_connectTagEvents:function(){
this.editor.onLoadDeferred.addCallback(_5.hitch(this,function(){
this.connect(this.editor.editNode,"ondblclick",this._onDblClick);
}));
},_isValid:function(){
return this._urlInput.isValid()&&this._textInput.isValid();
},_setContent:function(_1b){
this.dropDown.set({parserScope:"dojo",content:_1b});
},_checkValues:function(_1c){
if(_1c&&_1c.urlInput){
_1c.urlInput=_1c.urlInput.replace(/"/g,"&quot;");
}
return _1c;
},setValue:function(_1d){
this._onCloseDialog();
if(_6("ie")<9){
var sel=_c.getSelection(this.editor.window);
var _1e=sel.getRangeAt(0);
var a=_1e.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!==this.tag)){
a=_8.withGlobal(this.editor.window,"getSelectedElement",_d,[this.tag]);
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()===this.tag)){
if(this.editor.queryCommandEnabled("unlink")){
_8.withGlobal(this.editor.window,"selectElementChildren",_d,[a]);
this.editor.execCommand("unlink");
}
}
}
_1d=this._checkValues(_1d);
this.editor.execCommand("inserthtml",_7.substitute(this.htmlTemplate,_1d));
},_onCloseDialog:function(){
this.editor.focus();
},_getCurrentValues:function(a){
var url,_1f,_20;
if(a&&a.tagName.toLowerCase()===this.tag){
url=a.getAttribute("_djrealurl")||a.getAttribute("href");
_20=a.getAttribute("target")||"_self";
_1f=a.textContent||a.innerText;
_8.withGlobal(this.editor.window,"selectElement",_d,[a,true]);
}else{
_1f=_8.withGlobal(this.editor.window,_d.getSelectedText);
}
return {urlInput:url||"",textInput:_1f||"",targetSelect:_20||""};
},_onOpenDialog:function(){
var a;
if(_6("ie")<9){
var sel=_c.getSelection(this.editor.window);
var _21=sel.getRangeAt(0);
a=_21.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!==this.tag)){
a=_8.withGlobal(this.editor.window,"getSelectedElement",_d,[this.tag]);
}
}else{
a=_8.withGlobal(this.editor.window,"getAncestorElement",_d,[this.tag]);
}
this.dropDown.reset();
this._setButton.set("disabled",true);
this.dropDown.set("value",this._getCurrentValues(a));
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag&&_3.get(t,"href")){
var _22=this.editor;
_8.withGlobal(_22.window,"selectElement",_d,[t]);
_22.onDisplayChanged();
if(_22._updateTimer){
clearTimeout(_22._updateTimer);
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
}});
var _24=_2("dijit._editor.plugins.ImgLinkDialog",[_e],{linkDialogTemplate:["<table><tr><td>","<label for='${id}_urlInput'>${url}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' regExp='${urlRegExp}' "+"required='true' id='${id}_urlInput' name='urlInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input data-dojo-type='dijit.form.ValidationTextBox' required='false' id='${id}_textInput' "+"name='textInput' data-dojo-props='intermediateChanges:true'/>","</td></tr><tr><td>","</td><td>","</td></tr><tr><td colspan='2'>","<button data-dojo-type='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button data-dojo-type='dijit.form.Button' type='button' id='${id}_cancelButton'>${buttonCancel}</button>","</td></tr></table>"].join(""),htmlTemplate:"<img src=\"${urlInput}\" _djrealurl=\"${urlInput}\" alt=\"${textInput}\" />",tag:"img",_getCurrentValues:function(img){
var url,_25;
if(img&&img.tagName.toLowerCase()===this.tag){
url=img.getAttribute("_djrealurl")||img.getAttribute("src");
_25=img.getAttribute("alt");
_8.withGlobal(this.editor.window,"selectElement",_d,[img,true]);
}else{
_25=_8.withGlobal(this.editor.window,_d.getSelectedText);
}
return {urlInput:url||"",textInput:_25||""};
},_isValid:function(){
return this._urlInput.isValid();
},_connectTagEvents:function(){
this.inherited(arguments);
this.editor.onLoadDeferred.addCallback(_5.hitch(this,function(){
this.connect(this.editor.editNode,"onmousedown",this._selectTag);
}));
},_selectTag:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag){
_8.withGlobal(this.editor.window,"selectElement",_d,[t]);
}
}
},_checkValues:function(_26){
if(_26&&_26.urlInput){
_26.urlInput=_26.urlInput.replace(/"/g,"&quot;");
}
if(_26&&_26.textInput){
_26.textInput=_26.textInput.replace(/"/g,"&quot;");
}
return _26;
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg===this.tag&&_3.get(t,"src")){
var _27=this.editor;
_8.withGlobal(_27.window,"selectElement",_d,[t]);
_27.onDisplayChanged();
if(_27._updateTimer){
clearTimeout(_27._updateTimer);
delete _27._updateTimer;
}
_27.onNormalizedDisplayChanged();
var _28=this.button;
setTimeout(function(){
_28.set("disabled",false);
_28.loadAndOpenDropDown().then(function(){
if(_28.dropDown.focus){
_28.dropDown.focus();
}
});
},10);
}
}
}});
_a.registry["createLink"]=function(){
return new _e({command:"createLink"});
};
_a.registry["insertImage"]=function(){
return new _24({command:"insertImage"});
};
_e.ImgLinkDialog=_24;
return _e;
});
