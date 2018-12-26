//>>built
define("dojox/editor/plugins/AutoSave",["dojo","dijit","dojox","dijit/_base/manager","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/Dialog","dijit/MenuItem","dijit/Menu","dijit/form/Button","dijit/form/ComboButton","dijit/form/ComboBox","dijit/form/_TextBoxMixin","dijit/form/TextBox","dijit/TooltipDialog","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojo/date/locale","dojo/i18n","dojo/string","dojox/editor/plugins/Save","dojo/i18n!dojox/editor/plugins/nls/AutoSave"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.AutoSave");
_1.declare("dojox.editor.plugins._AutoSaveSettingDialog",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{dialogTitle:"",dialogDescription:"",paramName:"",paramLabel:"",btnOk:"",btnCancel:"",widgetsInTemplate:true,templateString:"<span id='${dialogId}' class='dijit dijitReset dijitInline' tabindex='-1'>"+"<div dojoType='dijit.Dialog' title='${dialogTitle}' dojoAttachPoint='dialog' "+"class='dijitEditorAutoSaveSettingDialog'>"+"<div tabindex='-1'>${dialogDescription}</div>"+"<div tabindex='-1' class='dijitEditorAutoSaveSettingInputArea'>${paramName}</div>"+"<div class='dijitEditorAutoSaveSettingInputArea' tabindex='-1'>"+"<input class='textBox' dojoType='dijit.form.TextBox' id='${textBoxId}' required='false' intermediateChanges='true' "+"selectOnClick='true' required='true' dojoAttachPoint='intBox' "+"dojoAttachEvent='onKeyDown: _onKeyDown, onChange: _onChange'/>"+"<label class='dijitLeft dijitInline boxLabel' "+"for='${textBoxId}' tabindex='-1'>${paramLabel}</label>"+"</div>"+"<div class='dijitEditorAutoSaveSettingButtonArea' tabindex='-1'>"+"<button dojoType='dijit.form.Button' dojoAttachEvent='onClick: onOk'>${btnOk}</button>"+"<button dojoType='dijit.form.Button' dojoAttachEvent='onClick: onCancel'>${btnCancel}</button>"+"</div>"+"</div>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.dialogId=this.id+"_dialog";
this.textBoxId=this.id+"_textBox";
},show:function(){
if(this._value==""){
this._value=0;
this.intBox.set("value",0);
}else{
this.intBox.set("value",this._value);
}
this.dialog.show();
_2.selectInputText(this.intBox.focusNode);
},hide:function(){
this.dialog.hide();
},onOk:function(){
this.dialog.hide();
},onCancel:function(){
this.dialog.hide();
},_onKeyDown:function(_4){
if(_4.keyCode==_1.keys.ENTER){
this.onOk();
}
},_onChange:function(_5){
if(this._isValidValue(_5)){
this._value=_5;
}else{
this.intBox.set("value",this._value);
}
},_setValueAttr:function(_6){
if(this._isValidValue(_6)){
this._value=_6;
}
},_getValueAttr:function(){
return this._value;
},_isValidValue:function(_7){
var _8=/^\d{0,3}$/,_9=String(_7);
return Boolean(_9.match?_9.match(_8):"");
}});
_1.declare("dojox.editor.plugins.AutoSave",_3.editor.plugins.Save,{url:"",logResults:true,interval:0,_iconClassPrefix:"dijitEditorIconAutoSave",_MIN:60000,_setIntervalAttr:function(_a){
this.interval=_a;
},_getIntervalAttr:function(){
return this._interval;
},setEditor:function(_b){
this.editor=_b;
this._strings=_1.i18n.getLocalization("dojox.editor.plugins","AutoSave");
this._initButton();
this._saveSettingDialog=new _3.editor.plugins._AutoSaveSettingDialog({"dialogTitle":this._strings["saveSettingdialogTitle"],"dialogDescription":this._strings["saveSettingdialogDescription"],"paramName":this._strings["saveSettingdialogParamName"],"paramLabel":this._strings["saveSettingdialogParamLabel"],"btnOk":this._strings["saveSettingdialogButtonOk"],"btnCancel":this._strings["saveSettingdialogButtonCancel"]});
this.connect(this._saveSettingDialog,"onOk","_onDialogOk");
var pd=(this._promDialog=new _2.TooltipDialog());
pd.startup();
pd.set("content","");
},_initButton:function(){
var _c=new _2.Menu({style:"display: none"}),_d=new _2.MenuItem({iconClass:this._iconClassPrefix+"Default "+this._iconClassPrefix,label:this._strings["saveLabel"]}),_e=(this._menuItemAutoSave=new _2.MenuItem({iconClass:this._iconClassPrefix+"Setting "+this._iconClassPrefix,label:this._strings["saveSettingLabelOn"]}));
_c.addChild(_d);
_c.addChild(_e);
this.button=new _2.form.ComboButton({label:this._strings["saveLabel"],iconClass:this._iconClassPrefix+"Default "+this._iconClassPrefix,showLabel:false,dropDown:_c});
this.connect(this.button,"onClick","_save");
this.connect(_d,"onClick","_save");
this._menuItemAutoSaveClickHandler=_1.connect(_e,"onClick",this,"_showAutSaveSettingDialog");
},_showAutSaveSettingDialog:function(){
var _f=this._saveSettingDialog;
_f.set("value",this.interval);
_f.show();
},_onDialogOk:function(){
var _10=(this.interval=this._saveSettingDialog.get("value")*this._MIN);
if(_10>0){
this._setSaveInterval(_10);
_1.disconnect(this._menuItemAutoSaveClickHandler);
this._menuItemAutoSave.set("label",this._strings["saveSettingLabelOff"]);
this._menuItemAutoSaveClickHandler=_1.connect(this._menuItemAutoSave,"onClick",this,"_onStopClick");
this.button.set("iconClass",this._iconClassPrefix+"Setting "+this._iconClassPrefix);
}
},_onStopClick:function(){
this._clearSaveInterval();
_1.disconnect(this._menuItemAutoSaveClickHandler);
this._menuItemAutoSave.set("label",this._strings["saveSettingLabelOn"]);
this._menuItemAutoSaveClickHandler=_1.connect(this._menuItemAutoSave,"onClick",this,"_showAutSaveSettingDialog");
this.button.set("iconClass",this._iconClassPrefix+"Default "+this._iconClassPrefix);
},_setSaveInterval:function(_11){
if(_11<=0){
return;
}
this._clearSaveInterval();
this._intervalHandler=setInterval(_1.hitch(this,function(){
if(!this._isWorking&&!this.get("disabled")){
this._isWorking=true;
this._save();
}
}),_11);
},_clearSaveInterval:function(){
if(this._intervalHandler){
clearInterval(this._intervalHandler);
this._intervalHandler=null;
}
},onSuccess:function(_12,_13){
this.button.set("disabled",false);
this._promDialog.set("content",_1.string.substitute(this._strings["saveMessageSuccess"],{"0":_1.date.locale.format(new Date(),{selector:"time"})}));
_2.popup.open({popup:this._promDialog,around:this.button.domNode});
this._promDialogTimeout=setTimeout(_1.hitch(this,function(){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}),3000);
this._isWorking=false;
if(this.logResults){
}
},onError:function(_14,_15){
this.button.set("disabled",false);
this._promDialog.set("content",_1.string.substitute(this._strings["saveMessageFail"],{"0":_1.date.locale.format(new Date(),{selector:"time"})}));
_2.popup.open({popup:this._promDialog,around:this.button.domNode});
this._promDialogTimeout=setTimeout(_1.hitch(this,function(){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}),3000);
this._isWorking=false;
if(this.logResults){
}
},destroy:function(){
this.inherited(arguments);
this._menuItemAutoSave=null;
if(this._promDialogTimeout){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}
this._clearSaveInterval();
if(this._saveSettingDialog){
this._saveSettingDialog.destroyRecursive();
this._destroyRecursive=null;
}
if(this._menuItemAutoSaveClickHandler){
_1.disconnect(this._menuItemAutoSaveClickHandler);
this._menuItemAutoSaveClickHandler=null;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _16=o.args.name.toLowerCase();
if(_16=="autosave"){
o.plugin=new _3.editor.plugins.AutoSave({url:("url" in o.args)?o.args.url:"",logResults:("logResults" in o.args)?o.args.logResults:true,interval:("interval" in o.args)?o.args.interval:5});
}
});
return _3.editor.plugins.AutoSave;
});
