//>>built
define("dojox/editor/plugins/Save",["dojo","dijit","dojox","dijit/form/Button","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Save"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.Save",_2._editor._Plugin,{iconClassPrefix:"dijitAdditionalEditorIcon",url:"",logResults:true,_initButton:function(){
var _4=_1.i18n.getLocalization("dojox.editor.plugins","Save");
this.button=new _2.form.Button({label:_4["save"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Save",tabIndex:"-1",onClick:_1.hitch(this,"_save")});
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_5){
this.editor=_5;
this._initButton();
},_save:function(){
var _6=this.editor.get("value");
this.save(_6);
},save:function(_7){
var _8={"Content-Type":"text/html"};
if(this.url){
var _9={url:this.url,postData:_7,headers:_8,handleAs:"text"};
this.button.set("disabled",true);
var _a=_1.xhrPost(_9);
_a.addCallback(_1.hitch(this,this.onSuccess));
_a.addErrback(_1.hitch(this,this.onError));
}else{
}
},onSuccess:function(_b,_c){
this.button.set("disabled",false);
if(this.logResults){
}
},onError:function(_d,_e){
this.button.set("disabled",false);
if(this.logResults){
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _f=o.args.name.toLowerCase();
if(_f==="save"){
o.plugin=new _3.editor.plugins.Save({url:("url" in o.args)?o.args.url:"",logResults:("logResults" in o.args)?o.args.logResults:true});
}
});
return _3.editor.plugins.Save;
});
