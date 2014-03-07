//>>built
define("dojox/editor/plugins/Preview",["dojo","dijit","dojox","dijit/form/Button","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Preview"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.Preview",_2._editor._Plugin,{useDefaultCommand:false,styles:"",stylesheets:null,iconClassPrefix:"dijitAdditionalEditorIcon",_initButton:function(){
this._nlsResources=_1.i18n.getLocalization("dojox.editor.plugins","Preview");
this.button=new _2.form.Button({label:this._nlsResources["preview"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Preview",tabIndex:"-1",onClick:_1.hitch(this,"_preview")});
},setEditor:function(_4){
this.editor=_4;
this._initButton();
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},_preview:function(){
try{
var _5=this.editor.get("value");
var _6="\t\t<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>\n";
var i;
if(this.stylesheets){
for(i=0;i<this.stylesheets.length;i++){
_6+="\t\t<link rel='stylesheet' type='text/css' href='"+this.stylesheets[i]+"'>\n";
}
}
if(this.styles){
_6+=("\t\t<style>"+this.styles+"</style>\n");
}
_5="<html>\n\t<head>\n"+_6+"\t</head>\n\t<body>\n"+_5+"\n\t</body>\n</html>";
var _7=window.open("javascript: ''",this._nlsResources["preview"],"status=1,menubar=0,location=0,toolbar=0");
_7.document.open();
_7.document.write(_5);
_7.document.close();
}
catch(e){
console.warn(e);
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _8=o.args.name.toLowerCase();
if(_8==="preview"){
o.plugin=new _3.editor.plugins.Preview({styles:("styles" in o.args)?o.args.styles:"",stylesheets:("stylesheets" in o.args)?o.args.stylesheets:null});
}
});
return _3.editor.plugins.Preview;
});
