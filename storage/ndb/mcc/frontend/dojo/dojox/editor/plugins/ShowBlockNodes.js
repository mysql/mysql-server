//>>built
define("dojox/editor/plugins/ShowBlockNodes",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/ShowBlockNodes"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.ShowBlockNodes",_4,{useDefaultCommand:false,iconClassPrefix:"dijitAdditionalEditorIcon",_styled:false,_initButton:function(){
var _6=_1.i18n.getLocalization("dojox.editor.plugins","ShowBlockNodes");
this.button=new _2.form.ToggleButton({label:_6["showBlockNodes"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ShowBlockNodes",tabIndex:"-1",onChange:_1.hitch(this,"_showBlocks")});
this.editor.addKeyHandler(_1.keys.F9,true,true,_1.hitch(this,this.toggle));
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_showBlocks:function(_8){
var _9=this.editor.document;
if(!this._styled){
try{
this._styled=true;
var _a="";
var _b=["div","p","ul","ol","table","h1","h2","h3","h4","h5","h6","pre","dir","center","blockquote","form","fieldset","address","object","pre","hr","ins","noscript","li","map","button","dd","dt"];
var _c="@media screen {\n"+"\t.editorShowBlocks {TAG} {\n"+"\t\tbackground-image: url({MODURL}/images/blockelems/{TAG}.gif);\n"+"\t\tbackground-repeat: no-repeat;\n"+"\t\tbackground-position: top left;\n"+"\t\tborder-width: 1px;\n"+"\t\tborder-style: dashed;\n"+"\t\tborder-color: #D0D0D0;\n"+"\t\tpadding-top: 15px;\n"+"\t\tpadding-left: 15px;\n"+"\t}\n"+"}\n";
_1.forEach(_b,function(_d){
_a+=_c.replace(/\{TAG\}/gi,_d);
});
var _e=_1.moduleUrl(_3._scopeName,"editor/plugins/resources").toString();
if(!(_e.match(/^https?:\/\//i))&&!(_e.match(/^file:\/\//i))){
var _f;
if(_e.charAt(0)==="/"){
var _10=_1.doc.location.protocol;
var _11=_1.doc.location.host;
_f=_10+"//"+_11;
}else{
_f=this._calcBaseUrl(_1.global.location.href);
}
if(_f[_f.length-1]!=="/"&&_e.charAt(0)!=="/"){
_f+="/";
}
_e=_f+_e;
}
_a=_a.replace(/\{MODURL\}/gi,_e);
if(!_1.isIE){
var _12=_9.createElement("style");
_12.appendChild(_9.createTextNode(_a));
_9.getElementsByTagName("head")[0].appendChild(_12);
}else{
var ss=_9.createStyleSheet("");
ss.cssText=_a;
}
}
catch(e){
console.warn(e);
}
}
if(_8){
_1.addClass(this.editor.editNode,"editorShowBlocks");
}else{
_1.removeClass(this.editor.editNode,"editorShowBlocks");
}
},_calcBaseUrl:function(_13){
var _14=null;
if(_13!==null){
var _15=_13.indexOf("?");
if(_15!=-1){
_13=_13.substring(0,_15);
}
_15=_13.lastIndexOf("/");
if(_15>0&&_15<_13.length){
_14=_13.substring(0,_15);
}else{
_14=_13;
}
}
return _14;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _16=o.args.name.toLowerCase();
if(_16==="showblocknodes"){
o.plugin=new _5();
}
});
return _5;
});
