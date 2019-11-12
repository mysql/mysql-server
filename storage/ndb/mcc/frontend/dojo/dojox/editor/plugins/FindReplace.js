//>>built
define("dojox/editor/plugins/FindReplace",["dojo","dijit","dojox","dijit/_base/manager","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_KeyNavContainer","dijit/_WidgetsInTemplateMixin","dijit/TooltipDialog","dijit/Toolbar","dijit/form/CheckBox","dijit/form/_TextBoxMixin","dijit/form/TextBox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/DropDownButton","dijit/form/ToggleButton","dojox/editor/plugins/ToolbarLineBreak","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/FindReplace"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
_1.experimental("dojox.editor.plugins.FindReplace");
_1.declare("dojox.editor.plugins._FindReplaceCloseBox",[_6,_7,_9],{btnId:"",widget:null,widgetsInTemplate:true,templateString:"<span style='float: right' class='dijitInline' tabindex='-1'>"+"<button class='dijit dijitReset dijitInline' "+"id='${btnId}' dojoAttachPoint='button' dojoType='dijit.form.Button' tabindex='-1' iconClass='dijitEditorIconsFindReplaceClose' showLabel='false'>X</button>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.btnId=this.id+"_close";
this.inherited(arguments);
},startup:function(){
this.connect(this.button,"onClick","onClick");
},onClick:function(){
}});
_1.declare("dojox.editor.plugins._FindReplaceTextBox",[_6,_7,_9],{textId:"",label:"",toolTip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline dijitEditorFindReplaceTextBox' "+"title='${tooltip}' tabindex='-1'>"+"<label class='dijitLeft dijitInline' for='${textId}' tabindex='-1'>${label}</label>"+"<input dojoType='dijit.form.TextBox' intermediateChanges='true' class='focusTextBox' "+"tabIndex='0' id='${textId}' dojoAttachPoint='textBox, focusNode' value='' dojoAttachEvent='onKeyPress: _onKeyPress'/>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.textId=this.id+"_text";
this.inherited(arguments);
},postCreate:function(){
this.textBox.set("value","");
this.disabled=this.textBox.get("disabled");
this.connect(this.textBox,"onChange","onChange");
_1.attr(this.textBox.textbox,"formnovalidate","true");
},_setValueAttr:function(_10){
this.value=_10;
this.textBox.set("value",_10);
},focus:function(){
this.textBox.focus();
},_setDisabledAttr:function(_11){
this.disabled=_11;
this.textBox.set("disabled",_11);
},onChange:function(val){
this.value=val;
},_onKeyPress:function(evt){
var _12=0;
var end=0;
if(evt.target&&!evt.ctrlKey&&!evt.altKey&&!evt.shiftKey){
if(evt.keyCode==_1.keys.LEFT_ARROW){
_12=evt.target.selectionStart;
end=evt.target.selectionEnd;
if(_12<end){
_2.selectInputText(evt.target,_12,_12);
_1.stopEvent(evt);
}
}else{
if(evt.keyCode==_1.keys.RIGHT_ARROW){
_12=evt.target.selectionStart;
end=evt.target.selectionEnd;
if(_12<end){
_2.selectInputText(evt.target,end,end);
_1.stopEvent(evt);
}
}
}
}
}});
_1.declare("dojox.editor.plugins._FindReplaceCheckBox",[_6,_7,_9],{checkId:"",label:"",tooltip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' tabindex='-1' "+"class='dijit dijitReset dijitInline dijitEditorFindReplaceCheckBox' title='${tooltip}' >"+"<input dojoType='dijit.form.CheckBox' "+"tabIndex='0' id='${checkId}' dojoAttachPoint='checkBox, focusNode' value=''/>"+"<label tabindex='-1' class='dijitLeft dijitInline' for='${checkId}'>${label}</label>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.checkId=this.id+"_check";
this.inherited(arguments);
},postCreate:function(){
this.checkBox.set("checked",false);
this.disabled=this.checkBox.get("disabled");
this.checkBox.isFocusable=function(){
return false;
};
},_setValueAttr:function(_13){
this.checkBox.set("value",_13);
},_getValueAttr:function(){
return this.checkBox.get("value");
},focus:function(){
this.checkBox.focus();
},_setDisabledAttr:function(_14){
this.disabled=_14;
this.checkBox.set("disabled",_14);
}});
_1.declare("dojox.editor.plugins._FindReplaceToolbar",_b,{postCreate:function(){
this.connectKeyNavHandlers([],[]);
this.connect(this.containerNode,"onclick","_onToolbarEvent");
this.connect(this.containerNode,"onkeydown","_onToolbarEvent");
_1.addClass(this.domNode,"dijitToolbar");
},addChild:function(_15,_16){
_2._KeyNavContainer.superclass.addChild.apply(this,arguments);
},_onToolbarEvent:function(evt){
evt.stopPropagation();
}});
_1.declare("dojox.editor.plugins.FindReplace",[_f],{buttonClass:_2.form.ToggleButton,iconClassPrefix:"dijitEditorIconsFindReplace",editor:null,button:null,_frToolbar:null,_closeBox:null,_findField:null,_replaceField:null,_findButton:null,_replaceButton:null,_replaceAllButton:null,_caseSensitive:null,_backwards:null,_promDialog:null,_promDialogTimeout:null,_strings:null,_bookmark:null,_initButton:function(){
this._strings=_1.i18n.getLocalization("dojox.editor.plugins","FindReplace");
this.button=new _2.form.ToggleButton({label:this._strings["findReplace"],showLabel:false,iconClass:this.iconClassPrefix+" dijitEditorIconFindString",tabIndex:"-1",onChange:_1.hitch(this,"_toggleFindReplace")});
if(_1.isOpera){
this.button.set("disabled",true);
}
this.connect(this.button,"set",_1.hitch(this,function(_17,val){
if(_17==="disabled"){
this._toggleFindReplace((!val&&this._displayed),true,true);
}
}));
},setEditor:function(_18){
this.editor=_18;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_toggleFindReplace:function(_19,_1a,_1b){
var _1c=_1.marginBox(this.editor.domNode);
if(_19&&!_1.isOpera){
_1.style(this._frToolbar.domNode,"display","block");
this._populateFindField();
if(!_1a){
this._displayed=true;
}
}else{
_1.style(this._frToolbar.domNode,"display","none");
if(!_1a){
this._displayed=false;
}
if(!_1b){
this.editor.focus();
}
}
this.editor.resize({h:_1c.h});
},_populateFindField:function(){
var ed=this.editor;
var win=ed.window;
var _1d=ed._sCall("getSelectedText",[null]);
if(this._findField&&this._findField.textBox){
if(_1d){
this._findField.textBox.set("value",_1d);
}
this._findField.textBox.focus();
_2.selectInputText(this._findField.textBox.focusNode);
}
},setToolbar:function(_1e){
this.inherited(arguments);
if(!_1.isOpera){
var _1f=(this._frToolbar=new _3.editor.plugins._FindReplaceToolbar());
_1.style(_1f.domNode,"display","none");
_1.place(_1f.domNode,_1e.domNode,"after");
_1f.startup();
this._closeBox=new _3.editor.plugins._FindReplaceCloseBox();
_1f.addChild(this._closeBox);
this._findField=new _3.editor.plugins._FindReplaceTextBox({label:this._strings["findLabel"],tooltip:this._strings["findTooltip"]});
_1f.addChild(this._findField);
this._replaceField=new _3.editor.plugins._FindReplaceTextBox({label:this._strings["replaceLabel"],tooltip:this._strings["replaceTooltip"]});
_1f.addChild(this._replaceField);
_1f.addChild(new _3.editor.plugins.ToolbarLineBreak());
this._findButton=new _2.form.Button({label:this._strings["findButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconFind"});
this._findButton.titleNode.title=this._strings["findButtonTooltip"];
_1f.addChild(this._findButton);
this._replaceButton=new _2.form.Button({label:this._strings["replaceButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplace"});
this._replaceButton.titleNode.title=this._strings["replaceButtonTooltip"];
_1f.addChild(this._replaceButton);
this._replaceAllButton=new _2.form.Button({label:this._strings["replaceAllButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplaceAll"});
this._replaceAllButton.titleNode.title=this._strings["replaceAllButtonTooltip"];
_1f.addChild(this._replaceAllButton);
this._caseSensitive=new _3.editor.plugins._FindReplaceCheckBox({label:this._strings["matchCase"],tooltip:this._strings["matchCaseTooltip"]});
_1f.addChild(this._caseSensitive);
this._backwards=new _3.editor.plugins._FindReplaceCheckBox({label:this._strings["backwards"],tooltip:this._strings["backwardsTooltip"]});
_1f.addChild(this._backwards);
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
var _20=this._findField.get("value");
if(_20){
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
},_find:function(_21){
var txt=this._findField.get("value")||"";
if(txt){
var _22=this._caseSensitive.get("value");
var _23=this._backwards.get("value");
var _24=this._findText(txt,_22,_23);
if(!_24&&_21){
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
return _24;
}
return false;
},_replace:function(_25){
var _26=false;
var ed=this.editor;
ed.focus();
var txt=this._findField.get("value")||"";
var _27=this._replaceField.get("value")||"";
if(txt){
var _28=this._caseSensitive.get("value");
var _29=this._backwards.get("value");
var _2a=ed._sCall("getSelectedText",[null]);
if(_1.isMoz){
txt=_1.trim(txt);
_2a=_1.trim(_2a);
}
var _2b=this._filterRegexp(txt,!_28);
if(_2a&&_2b.test(_2a)){
ed.execCommand("inserthtml",_27);
_26=true;
if(_29){
this._findText(_27,_28,_29);
ed._sCall("collapse",[true]);
}
}
if(!this._find(false)&&_25){
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
return _26;
}
return null;
},_replaceAll:function(_2c){
var _2d=0;
var _2e=this._backwards.get("value");
if(_2e){
this.editor.placeCursorAtEnd();
}else{
this.editor.placeCursorAtStart();
}
if(this._replace(false)){
_2d++;
}
var _2f=_1.hitch(this,function(){
if(this._replace(false)){
_2d++;
setTimeout(_2f,10);
}else{
if(_2c){
this._promDialog.set("content",_1.string.substitute(this._strings["replaceDialogText"],{"0":""+_2d}));
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
_2f();
},_findText:function(txt,_30,_31){
var ed=this.editor;
var win=ed.window;
var _32=false;
if(txt){
if(win.find){
_32=win.find(txt,_30,_31,false,false,false,false);
}else{
var doc=ed.document;
if(doc.selection||win.getSelection){
this.editor.focus();
var _33=doc.body.createTextRange();
var _34=_33.duplicate();
var _35=win.getSelection();
var _36=_35.getRangeAt(0);
var _37=doc.selection?doc.selection.createRange():null;
if(_37){
if(_31){
_33.setEndPoint("EndToStart",_37);
}else{
_33.setEndPoint("StartToEnd",_37);
}
}else{
if(this._bookmark){
var _38=win.getSelection().toString();
_33.moveToBookmark(this._bookmark);
if(_33.text!=_38){
_33=_34.duplicate();
this._bookmark=null;
}else{
if(_31){
_34.setEndPoint("EndToStart",_33);
_33=_34.duplicate();
}else{
_34.setEndPoint("StartToEnd",_33);
_33=_34.duplicate();
}
}
}
}
var _39=_30?4:0;
if(_31){
_39=_39|1;
}
_32=_33.findText(txt,_33.text.length,_39);
if(_32){
_33.select();
this._bookmark=_33.getBookmark();
}
}
}
}
return _32;
},_filterRegexp:function(_3a,_3b){
var rxp="";
var c=null;
for(var i=0;i<_3a.length;i++){
c=_3a.charAt(i);
switch(c){
case "\\":
rxp+=c;
i++;
rxp+=_3a.charAt(i);
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
if(_3b){
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
var _3c=o.args.name.toLowerCase();
if(_3c==="findreplace"){
o.plugin=new _3.editor.plugins.FindReplace({});
}
});
return _3.editor.plugins.FindReplace;
});
