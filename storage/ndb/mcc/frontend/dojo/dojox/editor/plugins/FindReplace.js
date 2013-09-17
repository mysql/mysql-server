//>>built
define("dojox/editor/plugins/FindReplace",["dojo","dijit","dojox","dijit/_base/manager","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_KeyNavContainer","dijit/_WidgetsInTemplateMixin","dijit/TooltipDialog","dijit/Toolbar","dijit/form/CheckBox","dijit/form/_TextBoxMixin","dijit/form/TextBox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/DropDownButton","dijit/form/ToggleButton","dojox/editor/plugins/ToolbarLineBreak","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/FindReplace"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.FindReplace");
_1.declare("dojox.editor.plugins._FindReplaceCloseBox",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{btnId:"",widget:null,widgetsInTemplate:true,templateString:"<span style='float: right' class='dijitInline' tabindex='-1'>"+"<button class='dijit dijitReset dijitInline' "+"id='${btnId}' dojoAttachPoint='button' dojoType='dijit.form.Button' tabindex='-1' iconClass='dijitEditorIconsFindReplaceClose' showLabel='false'>X</button>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.btnId=this.id+"_close";
this.inherited(arguments);
},startup:function(){
this.connect(this.button,"onClick","onClick");
},onClick:function(){
}});
_1.declare("dojox.editor.plugins._FindReplaceTextBox",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{textId:"",label:"",toolTip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline dijitEditorFindReplaceTextBox' "+"title='${tooltip}' tabindex='-1'>"+"<label class='dijitLeft dijitInline' for='${textId}' tabindex='-1'>${label}</label>"+"<input dojoType='dijit.form.TextBox' intermediateChanges='true' class='focusTextBox' "+"tabIndex='0' id='${textId}' dojoAttachPoint='textBox, focusNode' value='' dojoAttachEvent='onKeyPress: _onKeyPress'/>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.textId=this.id+"_text";
this.inherited(arguments);
},postCreate:function(){
this.textBox.set("value","");
this.disabled=this.textBox.get("disabled");
this.connect(this.textBox,"onChange","onChange");
_1.attr(this.textBox.textbox,"formnovalidate","true");
},_setValueAttr:function(_4){
this.value=_4;
this.textBox.set("value",_4);
},focus:function(){
this.textBox.focus();
},_setDisabledAttr:function(_5){
this.disabled=_5;
this.textBox.set("disabled",_5);
},onChange:function(_6){
this.value=_6;
},_onKeyPress:function(_7){
var _8=0;
var _9=0;
if(_7.target&&!_7.ctrlKey&&!_7.altKey&&!_7.shiftKey){
if(_7.keyCode==_1.keys.LEFT_ARROW){
_8=_7.target.selectionStart;
_9=_7.target.selectionEnd;
if(_8<_9){
_2.selectInputText(_7.target,_8,_8);
_1.stopEvent(_7);
}
}else{
if(_7.keyCode==_1.keys.RIGHT_ARROW){
_8=_7.target.selectionStart;
_9=_7.target.selectionEnd;
if(_8<_9){
_2.selectInputText(_7.target,_9,_9);
_1.stopEvent(_7);
}
}
}
}
}});
_1.declare("dojox.editor.plugins._FindReplaceCheckBox",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{checkId:"",label:"",tooltip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' tabindex='-1' "+"class='dijit dijitReset dijitInline dijitEditorFindReplaceCheckBox' title='${tooltip}' >"+"<input dojoType='dijit.form.CheckBox' "+"tabIndex='0' id='${checkId}' dojoAttachPoint='checkBox, focusNode' value=''/>"+"<label tabindex='-1' class='dijitLeft dijitInline' for='${checkId}'>${label}</label>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.checkId=this.id+"_check";
this.inherited(arguments);
},postCreate:function(){
this.checkBox.set("checked",false);
this.disabled=this.checkBox.get("disabled");
this.checkBox.isFocusable=function(){
return false;
};
},_setValueAttr:function(_a){
this.checkBox.set("value",_a);
},_getValueAttr:function(){
return this.checkBox.get("value");
},focus:function(){
this.checkBox.focus();
},_setDisabledAttr:function(_b){
this.disabled=_b;
this.checkBox.set("disabled",_b);
}});
_1.declare("dojox.editor.plugins._FindReplaceToolbar",_2.Toolbar,{postCreate:function(){
this.connectKeyNavHandlers([],[]);
this.connect(this.containerNode,"onclick","_onToolbarEvent");
this.connect(this.containerNode,"onkeydown","_onToolbarEvent");
_1.addClass(this.domNode,"dijitToolbar");
},addChild:function(_c,_d){
_2._KeyNavContainer.superclass.addChild.apply(this,arguments);
},_onToolbarEvent:function(_e){
_e.stopPropagation();
}});
_1.declare("dojox.editor.plugins.FindReplace",[_2._editor._Plugin],{buttonClass:_2.form.ToggleButton,iconClassPrefix:"dijitEditorIconsFindReplace",editor:null,button:null,_frToolbar:null,_closeBox:null,_findField:null,_replaceField:null,_findButton:null,_replaceButton:null,_replaceAllButton:null,_caseSensitive:null,_backwards:null,_promDialog:null,_promDialogTimeout:null,_strings:null,_initButton:function(){
this._strings=_1.i18n.getLocalization("dojox.editor.plugins","FindReplace");
this.button=new _2.form.ToggleButton({label:this._strings["findReplace"],showLabel:false,iconClass:this.iconClassPrefix+" dijitEditorIconFindString",tabIndex:"-1",onChange:_1.hitch(this,"_toggleFindReplace")});
if(_1.isOpera){
this.button.set("disabled",true);
}
this.connect(this.button,"set",_1.hitch(this,function(_f,val){
if(_f==="disabled"){
this._toggleFindReplace((!val&&this._displayed),true,true);
}
}));
},setEditor:function(_10){
this.editor=_10;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_toggleFindReplace:function(_11,_12,_13){
var _14=_1.marginBox(this.editor.domNode);
if(_11&&!_1.isOpera){
_1.style(this._frToolbar.domNode,"display","block");
this._populateFindField();
if(!_12){
this._displayed=true;
}
}else{
_1.style(this._frToolbar.domNode,"display","none");
if(!_12){
this._displayed=false;
}
if(!_13){
this.editor.focus();
}
}
this.editor.resize({h:_14.h});
},_populateFindField:function(){
var ed=this.editor;
var win=ed.window;
var _15=_1.withGlobal(ed.window,"getSelectedText",_2._editor.selection,[null]);
if(this._findField&&this._findField.textBox){
if(_15){
this._findField.textBox.set("value",_15);
}
this._findField.textBox.focus();
_2.selectInputText(this._findField.textBox.focusNode);
}
},setToolbar:function(_16){
this.inherited(arguments);
if(!_1.isOpera){
var _17=(this._frToolbar=new _3.editor.plugins._FindReplaceToolbar());
_1.style(_17.domNode,"display","none");
_1.place(_17.domNode,_16.domNode,"after");
_17.startup();
this._closeBox=new _3.editor.plugins._FindReplaceCloseBox();
_17.addChild(this._closeBox);
this._findField=new _3.editor.plugins._FindReplaceTextBox({label:this._strings["findLabel"],tooltip:this._strings["findTooltip"]});
_17.addChild(this._findField);
this._replaceField=new _3.editor.plugins._FindReplaceTextBox({label:this._strings["replaceLabel"],tooltip:this._strings["replaceTooltip"]});
_17.addChild(this._replaceField);
_17.addChild(new _3.editor.plugins.ToolbarLineBreak());
this._findButton=new _2.form.Button({label:this._strings["findButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconFind"});
this._findButton.titleNode.title=this._strings["findButtonTooltip"];
_17.addChild(this._findButton);
this._replaceButton=new _2.form.Button({label:this._strings["replaceButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplace"});
this._replaceButton.titleNode.title=this._strings["replaceButtonTooltip"];
_17.addChild(this._replaceButton);
this._replaceAllButton=new _2.form.Button({label:this._strings["replaceAllButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplaceAll"});
this._replaceAllButton.titleNode.title=this._strings["replaceAllButtonTooltip"];
_17.addChild(this._replaceAllButton);
this._caseSensitive=new _3.editor.plugins._FindReplaceCheckBox({label:this._strings["matchCase"],tooltip:this._strings["matchCaseTooltip"]});
_17.addChild(this._caseSensitive);
this._backwards=new _3.editor.plugins._FindReplaceCheckBox({label:this._strings["backwards"],tooltip:this._strings["backwardsTooltip"]});
_17.addChild(this._backwards);
this._findButton.set("disabled",true);
this._replaceButton.set("disabled",true);
this._replaceAllButton.set("disabled",true);
this.connect(this._findField,"onChange","_checkButtons");
this.connect(this._findField,"onKeyDown","_onFindKeyDown");
this.connect(this._replaceField,"onKeyDown","_onReplaceKeyDown");
this.connect(this._findButton,"onClick","_find");
this.connect(this._replaceButton,"onClick","_replace");
this.connect(this._replaceAllButton,"onClick","_replaceAll");
this.connect(this._closeBox,"onClick","toggle");
this._promDialog=new _2.TooltipDialog();
this._promDialog.startup();
this._promDialog.set("content","");
}
},_checkButtons:function(){
var _18=this._findField.get("value");
if(_18){
this._findButton.set("disabled",false);
this._replaceButton.set("disabled",false);
this._replaceAllButton.set("disabled",false);
}else{
this._findButton.set("disabled",true);
this._replaceButton.set("disabled",true);
this._replaceAllButton.set("disabled",true);
}
},_onFindKeyDown:function(evt){
if(evt.keyCode==_1.keys.ENTER){
this._find();
_1.stopEvent(evt);
}
},_onReplaceKeyDown:function(evt){
if(evt.keyCode==_1.keys.ENTER){
if(!this._replace()){
this._replace();
}
_1.stopEvent(evt);
}
},_find:function(_19){
var txt=this._findField.get("value")||"";
if(txt){
var _1a=this._caseSensitive.get("value");
var _1b=this._backwards.get("value");
var _1c=this._findText(txt,_1a,_1b);
if(!_1c&&_19){
this._promDialog.set("content",_1.string.substitute(this._strings["eofDialogText"],{"0":this._strings["eofDialogTextFind"]}));
_2.popup.open({popup:this._promDialog,around:this._findButton.domNode});
this._promDialogTimeout=setTimeout(_1.hitch(this,function(){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}),3000);
setTimeout(_1.hitch(this,function(){
this.editor.focus();
}),0);
}
return _1c;
}
return false;
},_replace:function(_1d){
var _1e=false;
var ed=this.editor;
ed.focus();
var txt=this._findField.get("value")||"";
var _1f=this._replaceField.get("value")||"";
if(txt){
var _20=this._caseSensitive.get("value");
var _21=this._backwards.get("value");
var _22=_1.withGlobal(ed.window,"getSelectedText",_2._editor.selection,[null]);
if(_1.isMoz){
txt=_1.trim(txt);
_22=_1.trim(_22);
}
var _23=this._filterRegexp(txt,!_20);
if(_22&&_23.test(_22)){
ed.execCommand("inserthtml",_1f);
_1e=true;
if(_21){
this._findText(_1f,_20,_21);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
}
if(!this._find(false)&&_1d){
this._promDialog.set("content",_1.string.substitute(this._strings["eofDialogText"],{"0":this._strings["eofDialogTextReplace"]}));
_2.popup.open({popup:this._promDialog,around:this._replaceButton.domNode});
this._promDialogTimeout=setTimeout(_1.hitch(this,function(){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}),3000);
setTimeout(_1.hitch(this,function(){
this.editor.focus();
}),0);
}
return _1e;
}
return null;
},_replaceAll:function(_24){
var _25=0;
var _26=this._backwards.get("value");
if(_26){
this.editor.placeCursorAtEnd();
}else{
this.editor.placeCursorAtStart();
}
if(this._replace(false)){
_25++;
}
var _27=_1.hitch(this,function(){
if(this._replace(false)){
_25++;
setTimeout(_27,10);
}else{
if(_24){
this._promDialog.set("content",_1.string.substitute(this._strings["replaceDialogText"],{"0":""+_25}));
_2.popup.open({popup:this._promDialog,around:this._replaceAllButton.domNode});
this._promDialogTimeout=setTimeout(_1.hitch(this,function(){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}),3000);
setTimeout(_1.hitch(this,function(){
this._findField.focus();
this._findField.textBox.focusNode.select();
}),0);
}
}
});
_27();
},_findText:function(txt,_28,_29){
var ed=this.editor;
var win=ed.window;
var _2a=false;
if(txt){
if(win.find){
_2a=win.find(txt,_28,_29,false,false,false,false);
}else{
var doc=ed.document;
if(doc.selection){
this.editor.focus();
var _2b=doc.body.createTextRange();
var _2c=doc.selection?doc.selection.createRange():null;
if(_2c){
if(_29){
_2b.setEndPoint("EndToStart",_2c);
}else{
_2b.setEndPoint("StartToEnd",_2c);
}
}
var _2d=_28?4:0;
if(_29){
_2d=_2d|1;
}
_2a=_2b.findText(txt,_2b.text.length,_2d);
if(_2a){
_2b.select();
}
}
}
}
return _2a;
},_filterRegexp:function(_2e,_2f){
var rxp="";
var c=null;
for(var i=0;i<_2e.length;i++){
c=_2e.charAt(i);
switch(c){
case "\\":
rxp+=c;
i++;
rxp+=_2e.charAt(i);
break;
case "$":
case "^":
case "/":
case "+":
case ".":
case "|":
case "(":
case ")":
case "{":
case "}":
case "[":
case "]":
rxp+="\\";
default:
rxp+=c;
}
}
rxp="^"+rxp+"$";
if(_2f){
return new RegExp(rxp,"mi");
}else{
return new RegExp(rxp,"m");
}
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},destroy:function(){
this.inherited(arguments);
if(this._promDialogTimeout){
clearTimeout(this._promDialogTimeout);
this._promDialogTimeout=null;
_2.popup.close(this._promDialog);
}
if(this._frToolbar){
this._frToolbar.destroyRecursive();
this._frToolbar=null;
}
if(this._promDialog){
this._promDialog.destroyRecursive();
this._promDialog=null;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _30=o.args.name.toLowerCase();
if(_30==="findreplace"){
o.plugin=new _3.editor.plugins.FindReplace({});
}
});
return _3.editor.plugins.FindReplace;
});
