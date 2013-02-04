//>>built
define("dojox/editor/plugins/ShowBlockNodes",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/ShowBlockNodes"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.ShowBlockNodes",_2._editor._Plugin,{useDefaultCommand:false,iconClassPrefix:"dijitAdditionalEditorIcon",_styled:false,_initButton:function(){
var _4=_1.i18n.getLocalization("dojox.editor.plugins","ShowBlockNodes");
this.button=new _2.form.ToggleButton({label:_4["showBlockNodes"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ShowBlockNodes",tabIndex:"-1",onChange:_1.hitch(this,"_showBlocks")});
this.editor.addKeyHandler(_1.keys.F9,true,true,_1.hitch(this,this.toggle));
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_5){
this.editor=_5;
this._initButton();
},toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_showBlocks:function(_6){
var _7=this.editor.document;
if(!this._styled){
try{
this._styled=true;
var _8="";
var _9=["div","p","ul","ol","table","h1","h2","h3","h4","h5","h6","pre","dir","center","blockquote","form","fieldset","address","object","pre","hr","ins","noscript","li","map","button","dd","dt"];
var _a="@media screen {\n"+"\t.editorShowBlocks {TAG} {\n"+"\t\tbackground-image: url({MODURL}/images/blockelems/{TAG}.gif);\n"+"\t\tbackground-repeat: no-repeat;\n"+"\t\tbackground-position: top left;\n"+"\t\tborder-width: 1px;\n"+"\t\tborder-style: dashed;\n"+"\t\tborder-color: #D0D0D0;\n"+"\t\tpadding-top: 15px;\n"+"\t\tpadding-left: 15px;\n"+"\t}\n"+"}\n";
_1.forEach(_9,function(_b){
_8+=_a.replace(/\{TAG\}/gi,_b);
});
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
_8=_8.replace(/\{MODURL\}/gi,_c);
if(!_1.isIE){
var _10=_7.createElement("style");
_10.appendChild(_7.createTextNode(_8));
_7.getElementsByTagName("head")[0].appendChild(_10);
}else{
var ss=_7.createStyleSheet("");
ss.cssText=_8;
}
}
catch(e){
console.warn(e);
}
}
if(_6){
_1.addClass(this.editor.editNode,"editorShowBlocks");
}else{
_1.removeClass(this.editor.editNode,"editorShowBlocks");
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
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _14=o.args.name.toLowerCase();
if(_14==="showblocknodes"){
o.plugin=new _3.editor.plugins.ShowBlockNodes();
}
});
return _3.editor.plugins.ShowBlockNodes;
});
