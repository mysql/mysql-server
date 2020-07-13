//>>built
define("dojox/editor/plugins/InsertAnchor",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/_base/manager","dijit/_editor/range","dijit/_Templated","dijit/TooltipDialog","dijit/form/ValidationTextBox","dijit/form/Select","dijit/form/Button","dijit/form/DropDownButton","dojo/_base/declare","dojo/i18n","dojo/string","dojo/NodeList-dom","dojox/editor/plugins/ToolbarLineBreak","dojo/i18n!dojox/editor/plugins/nls/InsertAnchor","dojo/i18n!dijit/nls/common"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.InsertAnchor",_4,{htmlTemplate:"<a name=\"${anchorInput}\" class=\"dijitEditorPluginInsertAnchorStyle\">${textInput}</a>",iconClassPrefix:"dijitAdditionalEditorIcon",_template:["<table role='presentation'><tr><td>","<label for='${id}_anchorInput'>${anchor}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' required='true' "+"id='${id}_anchorInput' name='anchorInput' intermediateChanges='true'>","</td></tr><tr><td>","<label for='${id}_textInput'>${text}</label>","</td><td>","<input dojoType='dijit.form.ValidationTextBox' required='true' id='${id}_textInput' "+"name='textInput' intermediateChanges='true'>","</td></tr>","<tr><td colspan='2'>","<button dojoType='dijit.form.Button' type='submit' id='${id}_setButton'>${set}</button>","<button dojoType='dijit.form.Button' type='button' id='${id}_cancelButton'>${cancel}</button>","</td></tr></table>"].join(""),_initButton:function(){
var _6=this;
var _7=_1.i18n.getLocalization("dojox.editor.plugins","InsertAnchor",this.lang);
var _8=(this.dropDown=new _2.TooltipDialog({title:_7["title"],execute:_1.hitch(this,"setValue"),onOpen:function(){
_6._onOpenDialog();
_2.TooltipDialog.prototype.onOpen.apply(this,arguments);
},onCancel:function(){
setTimeout(_1.hitch(_6,"_onCloseDialog"),0);
}}));
this.button=new _2.form.DropDownButton({label:_7["insertAnchor"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"InsertAnchor",tabIndex:"-1",dropDown:this.dropDown});
_7.id=_2.getUniqueId(this.editor.id);
this._uniqueId=_7.id;
this.dropDown.set("content",_8.title+"<div style='border-bottom: 1px black solid;padding-bottom:2pt;margin-bottom:4pt'></div>"+_1.string.substitute(this._template,_7));
_8.startup();
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
},setEditor:function(_9){
this.editor=_9;
this._initButton();
},_checkInput:function(){
var _a=true;
if(this._anchorInput.isValid()){
_a=false;
}
this._setButton.set("disabled",_a);
},_setup:function(){
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.connect(this.editor.editNode,"ondblclick",this._onDblClick);
setTimeout(_1.hitch(this,function(){
this._applyStyles();
}),100);
}));
},getAnchorStyle:function(){
var _b="@media screen {\n"+"\t.dijitEditorPluginInsertAnchorStyle {\n"+"\t\tbackground-image: url({MODURL}/images/anchor.gif);\n"+"\t\tbackground-repeat: no-repeat;\n"+"\t\tbackground-position: top left;\n"+"\t\tborder-width: 1px;\n"+"\t\tborder-style: dashed;\n"+"\t\tborder-color: #D0D0D0;\n"+"\t\tpadding-left: 20px;\n"+"\t}\n"+"}\n";
var _c=_1.moduleUrl(_3._scopeName,"editor/plugins/resources").toString();
if(!(_c.match(/^https?:\/\//i))&&!(_c.match(/^file:\/\//i))){
var _d;
if(_c.charAt(0)==="/"){
var _e=_1.doc.location.protocol;
var _f=_1.doc.location.host;
_d=_e+"//"+_f;
}else{
_d=this._calcBaseUrl(_1.global.location.href);
}
if(_d[_d.length-1]!=="/"&&_c.charAt(0)!=="/"){
_d+="/";
}
_c=_d+_c;
}
return _b.replace(/\{MODURL\}/gi,_c);
},_applyStyles:function(){
if(!this._styled){
try{
this._styled=true;
var doc=this.editor.document;
var _10=this.getAnchorStyle();
if(!_1.isIE){
var _11=doc.createElement("style");
_11.appendChild(doc.createTextNode(_10));
doc.getElementsByTagName("head")[0].appendChild(_11);
}else{
var ss=doc.createStyleSheet("");
ss.cssText=_10;
}
}
catch(e){
}
}
},_calcBaseUrl:function(_12){
var _13=null;
if(_12!==null){
var _14=_12.indexOf("?");
if(_14!=-1){
_12=_12.substring(0,_14);
}
_14=_12.lastIndexOf("/");
if(_14>0&&_14<_12.length){
_13=_12.substring(0,_14);
}else{
_13=_12;
}
}
return _13;
},_checkValues:function(_15){
if(_15){
if(_15.anchorInput){
_15.anchorInput=_15.anchorInput.replace(/"/g,"&quot;");
}
if(!_15.textInput){
_15.textInput="&nbsp;";
}
}
return _15;
},setValue:function(_16){
this._onCloseDialog();
if(!this.editor.window.getSelection){
var sel=_2.range.getSelection(this.editor.window);
var _17=sel.getRangeAt(0);
var a=_17.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!=="a")){
a=this.editor._sCall("getSelectedElement",["a"]);
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()==="a")){
if(this.editor.queryCommandEnabled("unlink")){
this.editor._sCall("selectElementChildren",[a]);
this.editor.execCommand("unlink");
}
}
}
_16=this._checkValues(_16);
this.editor.execCommand("inserthtml",_1.string.substitute(this.htmlTemplate,_16));
},_onCloseDialog:function(){
this.editor.focus();
},_getCurrentValues:function(a){
var _18,_19;
if(a&&a.tagName.toLowerCase()==="a"&&_1.attr(a,"name")){
_18=_1.attr(a,"name");
_19=a.textContent||a.innerText;
this.editor._sCall("selectElement",[a,true]);
}else{
_19=this.editor._sCall("getSelectedText");
}
return {anchorInput:_18||"",textInput:_19||""};
},_onOpenDialog:function(){
var a;
if(!this.editor.window.getSelection){
var sel=_2.range.getSelection(this.editor.window);
var _1a=sel.getRangeAt(0);
a=_1a.endContainer;
if(a.nodeType===3){
a=a.parentNode;
}
if(a&&(a.nodeName&&a.nodeName.toLowerCase()!=="a")){
a=this.editor._sCall("getSelectedElement",["a"]);
}
}else{
a=this.editor._sCall("getAncestorElement",["a"]);
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
this.editor._sCall("selectElement",[t]);
setTimeout(_1.hitch(this,function(){
this.button.set("disabled",false);
this.button.openDropDown();
if(this.button.dropDown.focus){
this.button.dropDown.focus();
}
}),10);
}
}
},_preDomFilter:function(_1b){
_1.query("a[name]:not([href])",this.editor.editNode).addClass("dijitEditorPluginInsertAnchorStyle");
},_postDomFilter:function(_1c){
if(_1c){
_1.query("a[name]:not([href])",_1c).removeClass("dijitEditorPluginInsertAnchorStyle");
}
return _1c;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _1d=o.args.name;
if(_1d){
_1d=_1d.toLowerCase();
}
if(_1d==="insertanchor"){
o.plugin=new _5();
}
});
return _5;
});
