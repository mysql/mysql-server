//>>built
define("dojox/editor/plugins/FindReplace",["dojo","dijit","dojox","dijit/_base/manager","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_KeyNavContainer","dijit/_WidgetsInTemplateMixin","dijit/TooltipDialog","dijit/Toolbar","dijit/form/CheckBox","dijit/form/_TextBoxMixin","dijit/form/TextBox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/DropDownButton","dijit/form/ToggleButton","./ToolbarLineBreak","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/FindReplace"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
_1.experimental("dojox.editor.plugins.FindReplace");
var _10=_1.declare("dojox.editor.plugins._FindReplaceCloseBox",[_6,_7,_9],{btnId:"",widget:null,widgetsInTemplate:true,templateString:"<span style='float: right' class='dijitInline' tabindex='-1'>"+"<button class='dijit dijitReset dijitInline' "+"id='${btnId}' dojoAttachPoint='button' dojoType='dijit.form.Button' tabindex='-1' iconClass='dijitEditorIconsFindReplaceClose' showLabel='false'>X</button>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.btnId=this.id+"_close";
this.inherited(arguments);
},startup:function(){
this.connect(this.button,"onClick","onClick");
},onClick:function(){
}});
var _11=_1.declare("dojox.editor.plugins._FindReplaceTextBox",[_6,_7,_9],{textId:"",label:"",toolTip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline dijitEditorFindReplaceTextBox' "+"title='${tooltip}' tabindex='-1'>"+"<label class='dijitLeft dijitInline' for='${textId}' tabindex='-1'>${label}</label>"+"<input dojoType='dijit.form.TextBox' intermediateChanges='true' class='focusTextBox' "+"tabIndex='0' id='${textId}' dojoAttachPoint='textBox, focusNode' value='' dojoAttachEvent='onKeyPress: _onKeyPress'/>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.textId=this.id+"_text";
this.inherited(arguments);
},postCreate:function(){
this.textBox.set("value","");
this.disabled=this.textBox.get("disabled");
this.connect(this.textBox,"onChange","onChange");
_1.attr(this.textBox.textbox,"formnovalidate","true");
},_setValueAttr:function(_12){
this.value=_12;
this.textBox.set("value",_12);
},focus:function(){
this.textBox.focus();
},_setDisabledAttr:function(_13){
this.disabled=_13;
this.textBox.set("disabled",_13);
},onChange:function(val){
this.value=val;
},_onKeyPress:function(evt){
var _14=0;
var end=0;
if(evt.target&&!evt.ctrlKey&&!evt.altKey&&!evt.shiftKey){
if(evt.keyCode==_1.keys.LEFT_ARROW){
_14=evt.target.selectionStart;
end=evt.target.selectionEnd;
if(_14<end){
_2.selectInputText(evt.target,_14,_14);
_1.stopEvent(evt);
}
}else{
if(evt.keyCode==_1.keys.RIGHT_ARROW){
_14=evt.target.selectionStart;
end=evt.target.selectionEnd;
if(_14<end){
_2.selectInputText(evt.target,end,end);
_1.stopEvent(evt);
}
}
}
}
}});
var _15=_1.declare("dojox.editor.plugins._FindReplaceCheckBox",[_6,_7,_9],{checkId:"",label:"",tooltip:"",widget:null,widgetsInTemplate:true,templateString:"<span style='white-space: nowrap' tabindex='-1' "+"class='dijit dijitReset dijitInline dijitEditorFindReplaceCheckBox' title='${tooltip}' >"+"<input dojoType='dijit.form.CheckBox' "+"tabIndex='0' id='${checkId}' dojoAttachPoint='checkBox, focusNode' value=''/>"+"<label tabindex='-1' class='dijitLeft dijitInline' for='${checkId}'>${label}</label>"+"</span>",postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.checkId=this.id+"_check";
this.inherited(arguments);
},postCreate:function(){
this.checkBox.set("checked",false);
this.disabled=this.checkBox.get("disabled");
this.checkBox.isFocusable=function(){
return false;
};
},_setValueAttr:function(_16){
this.checkBox.set("value",_16);
},_getValueAttr:function(){
return this.checkBox.get("value");
},focus:function(){
this.checkBox.focus();
},_setDisabledAttr:function(_17){
this.disabled=_17;
this.checkBox.set("disabled",_17);
}});
var _18=_1.declare("dojox.editor.plugins._FindReplaceToolbar",_b,{postCreate:function(){
this.connectKeyNavHandlers([],[]);
this.connect(this.containerNode,"onclick","_onToolbarEvent");
this.connect(this.containerNode,"onkeydown","_onToolbarEvent");
_1.addClass(this.domNode,"dijitToolbar");
},addChild:function(_19,_1a){
_2._KeyNavContainer.superclass.addChild.apply(this,arguments);
},_onToolbarEvent:function(evt){
evt.stopPropagation();
}});
var _1b=_1.declare("dojox.editor.plugins.FindReplace",[_f],{buttonClass:_2.form.ToggleButton,iconClassPrefix:"dijitEditorIconsFindReplace",editor:null,button:null,_frToolbar:null,_closeBox:null,_findField:null,_replaceField:null,_findButton:null,_replaceButton:null,_replaceAllButton:null,_caseSensitive:null,_backwards:null,_promDialog:null,_promDialogTimeout:null,_strings:null,_bookmark:null,_initButton:function(){
this._strings=_1.i18n.getLocalization("dojox.editor.plugins","FindReplace");
this.button=new _2.form.ToggleButton({label:this._strings["findReplace"],showLabel:false,iconClass:this.iconClassPrefix+" dijitEditorIconFindString",tabIndex:"-1",onChange:_1.hitch(this,"_toggleFindReplace")});
if(_1.isOpera){
this.button.set("disabled",true);
}
this.connect(this.button,"set",_1.hitch(this,function(_1c,val){
if(_1c==="disabled"){
this._toggleFindReplace((!val&&this._displayed),true,true);
}
}));
},setEditor:function(_1d){
this.editor=_1d;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_toggleFindReplace:function(_1e,_1f,_20){
var _21=_1.marginBox(this.editor.domNode);
if(_1e&&!_1.isOpera){
_1.style(this._frToolbar.domNode,"display","block");
this._populateFindField();
if(!_1f){
this._displayed=true;
}
}else{
_1.style(this._frToolbar.domNode,"display","none");
if(!_1f){
this._displayed=false;
}
if(!_20){
this.editor.focus();
}
}
this.editor.resize({h:_21.h});
},_populateFindField:function(){
var ed=this.editor;
var win=ed.window;
var _22=ed._sCall("getSelectedText",[null]);
if(this._findField&&this._findField.textBox){
if(_22){
this._findField.textBox.set("value",_22);
}
this._findField.textBox.focus();
_2.selectInputText(this._findField.textBox.focusNode);
}
},setToolbar:function(_23){
this.inherited(arguments);
if(!_1.isOpera){
var _24=(this._frToolbar=new _18());
_1.style(_24.domNode,"display","none");
_1.place(_24.domNode,_23.domNode,"after");
_24.startup();
this._closeBox=new _10();
_24.addChild(this._closeBox);
this._findField=new _11({label:this._strings["findLabel"],tooltip:this._strings["findTooltip"]});
_24.addChild(this._findField);
this._replaceField=new _11({label:this._strings["replaceLabel"],tooltip:this._strings["replaceTooltip"]});
_24.addChild(this._replaceField);
_24.addChild(new _3.editor.plugins.ToolbarLineBreak());
this._findButton=new _2.form.Button({label:this._strings["findButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconFind"});
this._findButton.titleNode.title=this._strings["findButtonTooltip"];
_24.addChild(this._findButton);
this._replaceButton=new _2.form.Button({label:this._strings["replaceButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplace"});
this._replaceButton.titleNode.title=this._strings["replaceButtonTooltip"];
_24.addChild(this._replaceButton);
this._replaceAllButton=new _2.form.Button({label:this._strings["replaceAllButton"],showLabel:true,iconClass:this.iconClassPrefix+" dijitEditorIconReplaceAll"});
this._replaceAllButton.titleNode.title=this._strings["replaceAllButtonTooltip"];
_24.addChild(this._replaceAllButton);
this._caseSensitive=new _15({label:this._strings["matchCase"],tooltip:this._strings["matchCaseTooltip"]});
_24.addChild(this._caseSensitive);
this._backwards=new _15({label:this._strings["backwards"],tooltip:this._strings["backwardsTooltip"]});
_24.addChild(this._backwards);
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
var _25=this._findField.get("value");
if(_25){
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
},_find:function(_26){
var txt=this._findField.get("value")||"";
if(txt){
var _27=this._caseSensitive.get("value");
var _28=this._backwards.get("value");
var _29=this._findText(txt,_27,_28);
if(!_29&&_26){
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
return _29;
}
return false;
},_replace:function(_2a){
var _2b=false;
var ed=this.editor;
ed.focus();
var txt=this._findField.get("value")||"";
var _2c=this._replaceField.get("value")||"";
if(txt){
var _2d=this._caseSensitive.get("value");
var _2e=this._backwards.get("value");
var _2f=ed._sCall("getSelectedText",[null]);
if(_1.isMoz){
txt=_1.trim(txt);
_2f=_1.trim(_2f);
}
var _30=this._filterRegexp(txt,!_2d);
if(_2f&&_30.test(_2f)){
ed.execCommand("inserthtml",_2c);
_2b=true;
if(_2e){
this._findText(_2c,_2d,_2e);
ed._sCall("collapse",[true]);
}
}
if(!this._find(false)&&_2a){
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
return _2b;
}
return null;
},_replaceAll:function(_31){
var _32=0;
var _33=this._backwards.get("value");
if(_33){
this.editor.placeCursorAtEnd();
}else{
this.editor.placeCursorAtStart();
}
if(this._replace(false)){
_32++;
}
var _34=_1.hitch(this,function(){
if(this._replace(false)){
_32++;
setTimeout(_34,10);
}else{
if(_31){
this._promDialog.set("content",_1.string.substitute(this._strings["replaceDialogText"],{"0":""+_32}));
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
_34();
},_findText:function(txt,_35,_36){
var ed=this.editor;
var win=ed.window;
var _37=false;
if(txt){
if(win.find){
_37=win.find(txt,_35,_36,false,false,false,false);
}else{
var doc=ed.document;
if(doc.selection||win.getSelection){
this.editor.focus();
var _38=doc.body.createTextRange();
var _39=_38.duplicate();
var _3a=win.getSelection();
var _3b=_3a.getRangeAt(0);
var _3c=doc.selection?doc.selection.createRange():null;
if(_3c){
if(_36){
_38.setEndPoint("EndToStart",_3c);
}else{
_38.setEndPoint("StartToEnd",_3c);
}
}else{
if(this._bookmark){
var _3d=win.getSelection().toString();
_38.moveToBookmark(this._bookmark);
if(_38.text!=_3d){
_38=_39.duplicate();
this._bookmark=null;
}else{
if(_36){
_39.setEndPoint("EndToStart",_38);
_38=_39.duplicate();
}else{
_39.setEndPoint("StartToEnd",_38);
_38=_39.duplicate();
}
}
}
}
var _3e=_35?4:0;
if(_36){
_3e=_3e|1;
}
_37=_38.findText(txt,_38.text.length,_3e);
if(_37){
_38.select();
this._bookmark=_38.getBookmark();
}
}
}
}
return _37;
},_filterRegexp:function(_3f,_40){
var rxp="";
var c=null;
for(var i=0;i<_3f.length;i++){
c=_3f.charAt(i);
switch(c){
case "\\":
rxp+=c;
i++;
rxp+=_3f.charAt(i);
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
if(_40){
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
_1b._FindReplaceCloseBox=_10;
_1b._FindReplaceTextBox=_11;
_1b._FindReplaceCheckBox=_15;
_1b._FindReplaceToolbar=_18;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _41=o.args.name.toLowerCase();
if(_41==="findreplace"){
o.plugin=new _1b({});
}
});
return _1b;
});
