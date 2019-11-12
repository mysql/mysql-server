//>>built
define("dojox/editor/plugins/ShowBlockNodes",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/ShowBlockNodes"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.ShowBlockNodes",_4,{useDefaultCommand:false,iconClassPrefix:"dijitAdditionalEditorIcon",_styled:false,_initButton:function(){
var _5=_1.i18n.getLocalization("dojox.editor.plugins","ShowBlockNodes");
this.button=new _2.form.ToggleButton({label:_5["showBlockNodes"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ShowBlockNodes",tabIndex:"-1",onChange:_1.hitch(this,"_showBlocks")});
this.editor.addKeyHandler(_1.keys.F9,true,true,_1.hitch(this,this.toggle));
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_6){
this.editor=_6;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_showBlocks:function(_7){
var _8=this.editor.document;
if(!this._styled){
try{
this._styled=true;
var _9="";
var _a=["div","p","ul","ol","table","h1","h2","h3","h4","h5","h6","pre","dir","center","blockquote","form","fieldset","address","object","pre","hr","ins","noscript","li","map","button","dd","dt"];
var _b="@media screen {\n"+"\t.editorShowBlocks {TAG} {\n"+"\t\tbackground-image: url({MODURL}/images/blockelems/{TAG}.gif);\n"+"\t\tbackground-repeat: no-repeat;\n"+"\t\tbackground-position: top left;\n"+"\t\tborder-width: 1px;\n"+"\t\tborder-style: dashed;\n"+"\t\tborder-color: #D0D0D0;\n"+"\t\tpadding-top: 15px;\n"+"\t\tpadding-left: 15px;\n"+"\t}\n"+"}\n";
_1.forEach(_a,function(_c){
_9+=_b.replace(/\{TAG\}/gi,_c);
});
var _d=_1.moduleUrl(_3._scopeName,"editor/plugins/resources").toString();
if(!(_d.match(/^https?:\/\//i))&&!(_d.match(/^file:\/\//i))){
var _e;
if(_d.charAt(0)==="/"){
var _f=_1.doc.location.protocol;
var _10=_1.doc.location.host;
_e=_f+"//"+_10;
}else{
_e=this._calcBaseUrl(_1.global.location.href);
}
if(_e[_e.length-1]!=="/"&&_d.charAt(0)!=="/"){
_e+="/";
}
_d=_e+_d;
}
_9=_9.replace(/\{MODURL\}/gi,_d);
if(!_1.isIE){
var _11=_8.createElement("style");
_11.appendChild(_8.createTextNode(_9));
_8.getElementsByTagName("head")[0].appendChild(_11);
}else{
var ss=_8.createStyleSheet("");
ss.cssText=_9;
}
}
catch(e){
console.warn(e);
}
}
if(_7){
_1.addClass(this.editor.editNode,"editorShowBlocks");
}else{
_1.removeClass(this.editor.editNode,"editorShowBlocks");
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
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _15=o.args.name.toLowerCase();
if(_15==="showblocknodes"){
o.plugin=new _3.editor.plugins.ShowBlockNodes();
}
});
return _3.editor.plugins.ShowBlockNodes;
});
