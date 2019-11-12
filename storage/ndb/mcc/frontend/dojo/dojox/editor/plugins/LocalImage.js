//>>built
define("dojox/editor/plugins/LocalImage",["dojo","dijit","dijit/registry","dijit/_base/popup","dijit/_editor/_Plugin","dijit/_editor/plugins/LinkDialog","dijit/TooltipDialog","dijit/form/_TextBoxMixin","dijit/form/Button","dijit/form/ValidationTextBox","dijit/form/DropDownButton","dojo/_base/connect","dojo/_base/declare","dojo/_base/sniff","dojox/form/FileUploader","dojo/i18n!dojox/editor/plugins/nls/LocalImage"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_1.declare("dojox.editor.plugins.LocalImage",_6.ImgLinkDialog,{uploadable:false,uploadUrl:"",baseImageUrl:"",fileMask:"*.jpg;*.jpeg;*.gif;*.png;*.bmp",urlRegExp:"",htmlFieldName:"uploadedfile",_isLocalFile:false,_messages:"",_cssPrefix:"dijitEditorEilDialog",_closable:true,linkDialogTemplate:["<div style='border-bottom: 1px solid black; padding-bottom: 2pt; margin-bottom: 4pt;'></div>","<div class='dijitEditorEilDialogDescription'>${prePopuTextUrl}${prePopuTextBrowse}</div>","<table role='presentation'><tr><td colspan='2'>","<label for='${id}_urlInput' title='${prePopuTextUrl}${prePopuTextBrowse}'>${url}</label>","</td></tr><tr><td class='dijitEditorEilDialogField'>","<input dojoType='dijit.form.ValidationTextBox' class='dijitEditorEilDialogField'"+"regExp='${urlRegExp}' title='${prePopuTextUrl}${prePopuTextBrowse}'  selectOnClick='true' required='true' "+"id='${id}_urlInput' name='urlInput' intermediateChanges='true' invalidMessage='${invalidMessage}' "+"prePopuText='&lt;${prePopuTextUrl}${prePopuTextBrowse}&gt'>","</td><td>","<div id='${id}_browse' style='display:${uploadable}'>${browse}</div>","</td></tr><tr><td colspan='2'>","<label for='${id}_textInput'>${text}</label>","</td></tr><tr><td>","<input dojoType='dijit.form.TextBox' required='false' id='${id}_textInput' "+"name='textInput' intermediateChanges='true' selectOnClick='true' class='dijitEditorEilDialogField'>","</td><td></td></tr><tr><td>","</td><td>","</td></tr><tr><td colspan='2'>","<button dojoType='dijit.form.Button' id='${id}_setButton'>${set}</button>","</td></tr></table>"].join(""),_initButton:function(){
var _12=this;
this._messages=_10;
this.tag="img";
var _13=(this.dropDown=new _7({title:_10[this.command+"Title"],onOpen:function(){
_12._initialFileUploader();
_12._onOpenDialog();
_7.prototype.onOpen.apply(this,arguments);
setTimeout(function(){
_8.selectInputText(_12._urlInput.textbox);
_12._urlInput.isLoadComplete=true;
},0);
},onClose:function(){
_1.disconnect(_12.blurHandler);
_12.blurHandler=null;
this.onHide();
},onCancel:function(){
setTimeout(_1.hitch(_12,"_onCloseDialog"),0);
}}));
var _14=this.getLabel(this.command),_15=this.iconClassPrefix+" "+this.iconClassPrefix+this.command.charAt(0).toUpperCase()+this.command.substr(1),_16=_1.mixin({label:_14,showLabel:false,iconClass:_15,dropDown:this.dropDown,tabIndex:"-1"},this.params||{});
if(!_e("ie")){
_16.closeDropDown=function(_17){
if(_12._closable){
if(this._opened){
_4.close(this.dropDown);
if(_17){
this.focus();
}
this._opened=false;
this.state="";
}
}
setTimeout(function(){
_12._closable=true;
},10);
};
}
this.button=new _b(_16);
var _18=this.fileMask.split(";"),_19="";
_1.forEach(_18,function(m){
m=m.replace(/\./,"\\.").replace(/\*/g,".*");
_19+="|"+m+"|"+m.toUpperCase();
});
_10.urlRegExp=this.urlRegExp=_19.substring(1);
if(!this.uploadable){
_10.prePopuTextBrowse=".";
}
_10.id=_3.getUniqueId(this.editor.id);
_10.uploadable=this.uploadable?"inline":"none";
this._uniqueId=_10.id;
this._setContent("<div class='"+this._cssPrefix+"Title'>"+_13.title+"</div>"+_1.string.substitute(this.linkDialogTemplate,_10));
_13.startup();
var _1a=(this._urlInput=_3.byId(this._uniqueId+"_urlInput"));
this._textInput=_3.byId(this._uniqueId+"_textInput");
this._setButton=_3.byId(this._uniqueId+"_setButton");
if(_1a){
var pt=_a.prototype;
_1a=_1.mixin(_1a,{isLoadComplete:false,isValid:function(_1b){
if(this.isLoadComplete){
return pt.isValid.apply(this,arguments);
}else{
return this.get("value").length>0;
}
},reset:function(){
this.isLoadComplete=false;
pt.reset.apply(this,arguments);
}});
this.connect(_1a,"onKeyDown","_cancelFileUpload");
this.connect(_1a,"onChange","_checkAndFixInput");
}
if(this._setButton){
this.connect(this._setButton,"onClick","_checkAndSetValue");
}
this._connectTagEvents();
},_initialFileUploader:function(){
var fup=null,_1c=this,_1d=_1c._uniqueId,_1e=_1d+"_browse",_1f=_1c._urlInput;
if(_1c.uploadable&&!_1c._fileUploader){
fup=_1c._fileUploader=new _f({force:"html",uploadUrl:_1c.uploadUrl,htmlFieldName:_1c.htmlFieldName,uploadOnChange:false,selectMultipleFiles:false,showProgress:true},_1e);
fup.reset=function(){
_1c._isLocalFile=false;
fup._resetHTML();
};
_1c.connect(fup,"onClick",function(){
_1f.validate(false);
if(!_e("ie")){
_1c._closable=false;
}
});
_1c.connect(fup,"onChange",function(_20){
_1c._isLocalFile=true;
_1f.set("value",_20[0].name);
_1f.focus();
});
_1c.connect(fup,"onComplete",function(_21){
var _22=_1c.baseImageUrl;
_22=_22&&_22.charAt(_22.length-1)=="/"?_22:_22+"/";
_1f.set("value",_22+_21[0].file);
_1c._isLocalFile=false;
_1c._setDialogStatus(true);
_1c.setValue(_1c.dropDown.get("value"));
});
_1c.connect(fup,"onError",function(_23){
_1c._setDialogStatus(true);
});
}
},_checkAndFixInput:function(){
this._setButton.set("disabled",!this._isValid());
},_isValid:function(){
return this._urlInput.isValid();
},_cancelFileUpload:function(){
this._fileUploader.reset();
this._isLocalFile=false;
},_checkAndSetValue:function(){
if(this._fileUploader&&this._isLocalFile){
this._setDialogStatus(false);
this._fileUploader.upload();
}else{
this.setValue(this.dropDown.get("value"));
}
},_setDialogStatus:function(_24){
this._urlInput.set("disabled",!_24);
this._textInput.set("disabled",!_24);
this._setButton.set("disabled",!_24);
},destroy:function(){
this.inherited(arguments);
if(this._fileUploader){
this._fileUploader.destroy();
delete this._fileUploader;
}
}});
var _25=function(_26){
return new _11({command:"insertImage",uploadable:("uploadable" in _26)?_26.uploadable:false,uploadUrl:("uploadable" in _26&&"uploadUrl" in _26)?_26.uploadUrl:"",htmlFieldName:("uploadable" in _26&&"htmlFieldName" in _26)?_26.htmlFieldName:"uploadedfile",baseImageUrl:("uploadable" in _26&&"baseImageUrl" in _26)?_26.baseImageUrl:"",fileMask:("fileMask" in _26)?_26.fileMask:"*.jpg;*.jpeg;*.gif;*.png;*.bmp"});
};
_5.registry["LocalImage"]=_25;
_5.registry["localImage"]=_25;
_5.registry["localimage"]=_25;
return _11;
});
