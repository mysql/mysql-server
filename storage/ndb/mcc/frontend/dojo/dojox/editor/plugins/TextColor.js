//>>built
define("dojox/editor/plugins/TextColor",["dojo","dijit","dojox","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/_editor/_Plugin","dijit/TooltipDialog","dijit/form/Button","dijit/form/DropDownButton","dojox/widget/ColorPicker","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/TextColor"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.editor.plugins.TextColor");
_1.declare("dojox.editor.plugins._TextColorDropDown",[_5,_6,_7],{templateString:"<div style='display: none; position: absolute; top: -10000; z-index: -10000'>"+"<div dojoType='dijit.TooltipDialog' dojoAttachPoint='dialog' class='dojoxEditorColorPicker'>"+"<div dojoType='dojox.widget.ColorPicker' dojoAttachPoint='_colorPicker'></div>"+"<br>"+"<center>"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_setButton'>${setButtonText}</button>"+"&nbsp;"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_cancelButton'>${cancelButtonText}</button>"+"</center>"+"</div>"+"</div>",widgetsInTemplate:true,constructor:function(){
var _9=_1.i18n.getLocalization("dojox.editor.plugins","TextColor");
_1.mixin(this,_9);
},startup:function(){
if(!this._started){
this.inherited(arguments);
this.connect(this._setButton,"onClick",_1.hitch(this,function(){
this.onChange(this.get("value"));
}));
this.connect(this._cancelButton,"onClick",_1.hitch(this,function(){
_2.popup.close(this.dialog);
this.onCancel();
}));
_1.style(this.domNode,"display","block");
}
},_setValueAttr:function(_a,_b){
this._colorPicker.set("value",_a,_b);
},_getValueAttr:function(){
return this._colorPicker.get("value");
},onChange:function(_c){
},onCancel:function(){
}});
_1.declare("dojox.editor.plugins.TextColor",_8,{buttonClass:_2.form.DropDownButton,useDefaultCommand:false,constructor:function(){
this._picker=new _3.editor.plugins._TextColorDropDown();
_1.body().appendChild(this._picker.domNode);
this._picker.startup();
this.dropDown=this._picker.dialog;
this.connect(this._picker,"onChange",function(_d){
this.editor.execCommand(this.command,_d);
});
this.connect(this._picker,"onCancel",function(){
this.editor.focus();
});
},updateState:function(){
var _e=this.editor;
var _f=this.command;
if(!_e||!_e.isLoaded||!_f.length){
return;
}
var _10=this.get("disabled");
var _11;
if(this.button){
this.button.set("disabled",_10);
if(_10){
return;
}
try{
_11=_e.queryCommandValue(_f)||"";
}
catch(e){
_11="";
}
}
if(_11==""){
_11="#000000";
}
if(_11=="transparent"){
_11="#ffffff";
}
if(typeof _11=="string"){
if(_11.indexOf("rgb")>-1){
_11=_1.colorFromRgb(_11).toHex();
}
}else{
_11=((_11&255)<<16)|(_11&65280)|((_11&16711680)>>>16);
_11=_11.toString(16);
_11="#000000".slice(0,7-_11.length)+_11;
}
if(_11!==this._picker.get("value")){
this._picker.set("value",_11,false);
}
},destroy:function(){
this.inherited(arguments);
this._picker.destroyRecursive();
delete this._picker;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
switch(o.args.name){
case "foreColor":
case "hiliteColor":
o.plugin=new _3.editor.plugins.TextColor({command:o.args.name});
}
});
return _3.editor.plugins.TextColor;
});
