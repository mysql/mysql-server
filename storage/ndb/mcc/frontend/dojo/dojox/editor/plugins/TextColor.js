//>>built
define("dojox/editor/plugins/TextColor",["dojo","dijit","dojox","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/_editor/_Plugin","dijit/TooltipDialog","dijit/form/Button","dijit/form/DropDownButton","dojox/widget/ColorPicker","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/TextColor"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.editor.plugins.TextColor");
var _9=_1.declare("dojox.editor.plugins._TextColorDropDown",[_5,_6,_7],{templateString:"<div style='display: none; position: absolute; top: -10000; z-index: -10000'>"+"<div dojoType='dijit.TooltipDialog' dojoAttachPoint='dialog' class='dojoxEditorColorPicker'>"+"<div dojoType='dojox.widget.ColorPicker' dojoAttachPoint='_colorPicker'></div>"+"<br>"+"<center>"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_setButton'>${setButtonText}</button>"+"&nbsp;"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_cancelButton'>${cancelButtonText}</button>"+"</center>"+"</div>"+"</div>",widgetsInTemplate:true,constructor:function(){
var _a=_1.i18n.getLocalization("dojox.editor.plugins","TextColor");
_1.mixin(this,_a);
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
},_setValueAttr:function(_b,_c){
this._colorPicker.set("value",_b,_c);
},_getValueAttr:function(){
return this._colorPicker.get("value");
},onChange:function(_d){
},onCancel:function(){
}});
var _e=_1.declare("dojox.editor.plugins.TextColor",_8,{buttonClass:_2.form.DropDownButton,useDefaultCommand:false,constructor:function(){
this._picker=new _9();
_1.body().appendChild(this._picker.domNode);
this._picker.startup();
this.dropDown=this._picker.dialog;
this.connect(this._picker,"onChange",function(_f){
this.editor.execCommand(this.command,_f);
});
this.connect(this._picker,"onCancel",function(){
this.editor.focus();
});
},updateState:function(){
var _10=this.editor;
var _11=this.command;
if(!_10||!_10.isLoaded||!_11.length){
return;
}
var _12=this.get("disabled");
var _13;
if(this.button){
this.button.set("disabled",_12);
if(_12){
return;
}
try{
_13=_10.queryCommandValue(_11)||"";
}
catch(e){
_13="";
}
}
if(_13==""){
_13="#000000";
}
if(_13=="transparent"){
_13="#ffffff";
}
if(typeof _13=="string"){
if(_13.indexOf("rgb")>-1){
_13=_1.colorFromRgb(_13).toHex();
}
}else{
_13=((_13&255)<<16)|(_13&65280)|((_13&16711680)>>>16);
_13=_13.toString(16);
_13="#000000".slice(0,7-_13.length)+_13;
}
if(_13!==this._picker.get("value")){
this._picker.set("value",_13,false);
}
},destroy:function(){
this.inherited(arguments);
this._picker.destroyRecursive();
delete this._picker;
}});
_e._TextColorDropDown=_9;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
switch(o.args.name){
case "foreColor":
case "hiliteColor":
o.plugin=new _e({command:o.args.name});
}
});
return _e;
});
