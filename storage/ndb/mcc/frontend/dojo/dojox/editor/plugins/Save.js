//>>built
define("dojox/editor/plugins/Save",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Save"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.Save",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",url:"",logResults:true,_initButton:function(){
var _5=_1.i18n.getLocalization("dojox.editor.plugins","Save");
this.button=new _2.form.Button({label:_5["save"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Save",tabIndex:"-1",onClick:_1.hitch(this,"_save")});
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_6){
this.editor=_6;
this._initButton();
},_save:function(){
var _7=this.editor.get("value");
this.save(_7);
},save:function(_8){
var _9={"Content-Type":"text/html"};
if(this.url){
var _a={url:this.url,postData:_8,headers:_9,handleAs:"text"};
this.button.set("disabled",true);
var _b=_1.xhrPost(_a);
_b.addCallback(_1.hitch(this,this.onSuccess));
_b.addErrback(_1.hitch(this,this.onError));
}else{
}
},onSuccess:function(_c,_d){
this.button.set("disabled",false);
if(this.logResults){
}
},onError:function(_e,_f){
this.button.set("disabled",false);
if(this.logResults){
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _10=o.args.name.toLowerCase();
if(_10==="save"){
o.plugin=new _3.editor.plugins.Save({url:("url" in o.args)?o.args.url:"",logResults:("logResults" in o.args)?o.args.logResults:true});
}
});
return _3.editor.plugins.Save;
});
