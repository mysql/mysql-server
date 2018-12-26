//>>built
define("dojox/editor/plugins/TextColor",["dojo","dijit","dojox","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/TooltipDialog","dijit/form/Button","dijit/form/DropDownButton","dijit/_editor/_Plugin","dojox/widget/ColorPicker","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/TextColor"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.TextColor");
_1.declare("dojox.editor.plugins._TextColorDropDown",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{templateString:"<div style='display: none; position: absolute; top: -10000; z-index: -10000'>"+"<div dojoType='dijit.TooltipDialog' dojoAttachPoint='dialog' class='dojoxEditorColorPicker'>"+"<div dojoType='dojox.widget.ColorPicker' dojoAttachPoint='_colorPicker'></div>"+"<br>"+"<center>"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_setButton'>${setButtonText}</button>"+"&nbsp;"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_cancelButton'>${cancelButtonText}</button>"+"</center>"+"</div>"+"</div>",widgetsInTemplate:true,constructor:function(){
var _4=_1.i18n.getLocalization("dojox.editor.plugins","TextColor");
_1.mixin(this,_4);
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
},_setValueAttr:function(_5,_6){
this._colorPicker.set("value",_5,_6);
},_getValueAttr:function(){
return this._colorPicker.get("value");
},onChange:function(_7){
},onCancel:function(){
}});
_1.declare("dojox.editor.plugins.TextColor",_2._editor._Plugin,{buttonClass:_2.form.DropDownButton,useDefaultCommand:false,constructor:function(){
this._picker=new _3.editor.plugins._TextColorDropDown();
_1.body().appendChild(this._picker.domNode);
this._picker.startup();
this.dropDown=this._picker.dialog;
this.connect(this._picker,"onChange",function(_8){
this.editor.execCommand(this.command,_8);
});
this.connect(this._picker,"onCancel",function(){
this.editor.focus();
});
},updateState:function(){
var _9=this.editor;
var _a=this.command;
if(!_9||!_9.isLoaded||!_a.length){
return;
}
var _b=this.get("disabled");
var _c;
if(this.button){
this.button.set("disabled",_b);
if(_b){
return;
}
try{
_c=_9.queryCommandValue(_a)||"";
}
catch(e){
_c="";
}
}
if(_c==""){
_c="#000000";
}
if(_c=="transparent"){
_c="#ffffff";
}
if(typeof _c=="string"){
if(_c.indexOf("rgb")>-1){
_c=_1.colorFromRgb(_c).toHex();
}
}else{
_c=((_c&255)<<16)|(_c&65280)|((_c&16711680)>>>16);
_c=_c.toString(16);
_c="#000000".slice(0,7-_c.length)+_c;
}
if(_c!==this._picker.get("value")){
this._picker.set("value",_c,false);
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
