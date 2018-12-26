//>>built
define("dojox/editor/plugins/InsertAnchor",["dojo","dijit","dojox","dijit/_base/manager","dijit/_editor/range","dijit/_Templated","dijit/TooltipDialog","dijit/form/ValidationTextBox","dijit/form/Select","dijit/form/Button","dijit/form/DropDownButton","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojox/editor/plugins/ToolbarLineBreak","dojo/i18n!dojox/editor/plugins/nls/InsertAnchor","dojo/i18n!dijit/nls/common"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.InsertAnchor",_2._editor._Plugin,{htmlTemplate:"<a name=\"${anchorInput}\" class=\"dijitEditorPluginInsertAnchorStyle\">${textInput}</a>",iconClassPrefix:"dijitAdditionalEditorIcon",_template:["<table><tr><td>","<label for='${id}_anchorInput'>${anchor}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' required='true' "+"id='${id}_anchorInput' name='anchorInput' intermediateChanges='true'>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' required='true' id='${id}_textInput' "+"name='textInput' intermediateChanges='true'>","</td></tr>","<tr><td colspan='2'>","<button dojoType='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button dojoType='dijit.form.Button' type='button' id='${id}_cancelButton'>${cancel}</button>","</td></tr></table>"].join(""),_initButton:function(){
var _4=this;
var _5=_1.i18n.getLocalization("dojox.editor.plugins","InsertAnchor",this.lang);
var _6=(this.dropDown=new _2.TooltipDialog({title:_5["title"],execute:_1.hitch(this,"setValue"),onOpen:function(){
_4._onOpenDialog();
_2.TooltipDialog.prototype.onOpen.apply(this,arguments);
},onCancel:function(){
setTimeout(_1.hitch(_4,"_onCloseDialog"),0);
}}));
this.button=new _2.form.DropDownButton({label:_5["insertAnchor"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"InsertAnchor",tabIndex:"-1",dropDown:this.dropDown});
_5.id=_2.getUniqueId(this.editor.id);
this._uniqueId=_5.id;
this.dropDown.set("content",_6.title+"<div style='border-bottom: 1px black solid;padding-bottom:2pt;margin-bottom:4pt'></div>"+_1.string.substitute(this._template,_5));
_6.startup();
this._anchorInput=_2.byId(this._uniqueId+"_anchorInput");
this._textInput=_2.byId(this._uniqueId+"_textInput");
this._setButton=_2.byId(this._uniqueId+"_setButton");
this.connect(_2.byId(this._uniqueId+"_cancelButton"),"onClick",function(){
this.dropDown.onCancel();
});
if(this._anchorInput){
this.connect(this._anchorInput,"onChange","_checkInput");
}
if(this._textInput){
this.connect(this._anchorInput,"onChange","_checkInput");
}
this.editor.contentDomPreFilters.push(_1.hitch(this,this._preDomFilter));
this.editor.contentDomPostFilters.push(_1.hitch(this,this._postDomFilter));
this._setup();
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
},_checkInput:function(){
var _8=true;
if(this._anchorInput.isValid()){
_8=false;
}
this._setButton.set("disabled",_8);
},_setup:function(){
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.connect(this.editor.editNode,"ondblclick",this._onDblClick);
setTimeout(_1.hitch(this,function(){
this._applyStyles();
}),100);
}));
},getAnchorStyle:function(){
var _9="@media screen {\n"+"\t.dijitEditorPluginInsertAnchorStyle {\n"+"\t\tbackground-image: url({MODURL}/images/anchor.gif);\n"+"\t\tbackground-repeat: no-repeat;\n"+"\t\tbackground-position: top left;\n"+"\t\tborder-width: 1px;\n"+"\t\tborder-style: dashed;\n"+"\t\tborder-color: #D0D0D0;\n"+"\t\tpadding-left: 20px;\n"+"\t}\n"+"}\n";
var _a=_1.moduleUrl(_3._scopeName,"editor/plugins/resources").toString();
if(!(_a.match(/^https?:\/\//i))&&!(_a.match(/^file:\/\//i))){
var _b;
if(_a.charAt(0)==="/"){
var _c=_1.doc.location.protocol;
var _d=_1.doc.location.host;
_b=_c+"//"+_d;
}else{
_b=this._calcBaseUrl(_1.global.location.href);
}
if(_b[_b.length-1]!=="/"&&_a.charAt(0)!=="/"){
_b+="/";
}
_a=_b+_a;
}
return _9.replace(/\{MODURL\}/gi,_a);
},_applyStyles:function(){
if(!this._styled){
try{
this._styled=true;
var _e=this.editor.document;
var _f=this.getAnchorStyle();
if(!_1.isIE){
var _10=_e.createElement("style");
_10.appendChild(_e.createTextNode(_f));
_e.getElementsByTagName("head")[0].appendChild(_10);
}else{
var ss=_e.createStyleSheet("");
ss.cssText=_f;
}
}
catch(e){
}
}
},_calcBaseUrl:function(_11){
var _12=null;
if(_11!==null){
var _13=_11.indexOf("?");
if(_13!=-1){
_11=_11.substring(0,_13);
}
_13=_11.lastIndexOf("/");
if(_13>0&&_13<_11.length){
_12=_11.substring(0,_13);
}else{
_12=_11;
}
}
return _12;
},_checkValues:function(_14){
if(_14){
if(_14.anchorInput){
_14.anchorInput=_14.anchorInput.replace(/"/g,"&quot;");
}
if(!_14.textInput){
_14.textInput="&nbsp;";
}
}
return _14;
},setValue:function(_15){
this._onCloseDialog();
if(!this.editor.window.getSelection){
var sel=_2.range.getSelection(this.editor.window);
var _16=sel.getRangeAt(0);
var a=_16.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!=="a")){
a=_1.withGlobal(this.editor.window,"getSelectedElement",_2._editor.selection,["a"]);
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()==="a")){
if(this.editor.queryCommandEnabled("unlink")){
_1.withGlobal(this.editor.window,"selectElementChildren",_2._editor.selection,[a]);
this.editor.execCommand("unlink");
}
}
}
_15=this._checkValues(_15);
this.editor.execCommand("inserthtml",_1.string.substitute(this.htmlTemplate,_15));
},_onCloseDialog:function(){
this.editor.focus();
},_getCurrentValues:function(a){
var _17,_18;
if(a&&a.tagName.toLowerCase()==="a"&&_1.attr(a,"name")){
_17=_1.attr(a,"name");
_18=a.textContent||a.innerText;
_1.withGlobal(this.editor.window,"selectElement",_2._editor.selection,[a,true]);
}else{
_18=_1.withGlobal(this.editor.window,_2._editor.selection.getSelectedText);
}
return {anchorInput:_17||"",textInput:_18||""};
},_onOpenDialog:function(){
var a;
if(!this.editor.window.getSelection){
var sel=_2.range.getSelection(this.editor.window);
var _19=sel.getRangeAt(0);
a=_19.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!=="a")){
a=_1.withGlobal(this.editor.window,"getSelectedElement",_2._editor.selection,["a"]);
}
}else{
a=_1.withGlobal(this.editor.window,"getAncestorElement",_2._editor.selection,["a"]);
}
this.dropDown.reset();
this._setButton.set("disabled",true);
this.dropDown.set("value",this._getCurrentValues(a));
},_onDblClick:function(e){
if(e&&e.target){
var t=e.target;
var tg=t.tagName?t.tagName.toLowerCase():"";
if(tg==="a"&&_1.attr(t,"name")){
this.editor.onDisplayChanged();
_1.withGlobal(this.editor.window,"selectElement",_2._editor.selection,[t]);
setTimeout(_1.hitch(this,function(){
this.button.set("disabled",false);
this.button.openDropDown();
if(this.button.dropDown.focus){
this.button.dropDown.focus();
}
}),10);
}
}
},_preDomFilter:function(_1a){
var ed=this.editor;
_1.withGlobal(ed.window,function(){
_1.query("a",ed.editNode).forEach(function(a){
if(_1.attr(a,"name")&&!_1.attr(a,"href")){
if(!_1.hasClass(a,"dijitEditorPluginInsertAnchorStyle")){
_1.addClass(a,"dijitEditorPluginInsertAnchorStyle");
}
}
});
});
},_postDomFilter:function(_1b){
var ed=this.editor;
_1.withGlobal(ed.window,function(){
_1.query("a",_1b).forEach(function(a){
if(_1.attr(a,"name")&&!_1.attr(a,"href")){
if(_1.hasClass(a,"dijitEditorPluginInsertAnchorStyle")){
_1.removeClass(a,"dijitEditorPluginInsertAnchorStyle");
}
}
});
});
return _1b;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _1c=o.args.name;
if(_1c){
_1c=_1c.toLowerCase();
}
if(_1c==="insertanchor"){
o.plugin=new _3.editor.plugins.InsertAnchor();
}
});
return _3.editor.plugins.InsertAnchor;
});
