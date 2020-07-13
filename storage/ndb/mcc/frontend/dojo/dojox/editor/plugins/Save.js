//>>built
define("dojox/editor/plugins/Save",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Save"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.Save",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",url:"",logResults:true,_initButton:function(){
var _6=_1.i18n.getLocalization("dojox.editor.plugins","Save");
this.button=new _2.form.Button({label:_6["save"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Save",tabIndex:"-1",onClick:_1.hitch(this,"_save")});
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
},_save:function(){
var _8=this.editor.get("value");
this.save(_8);
},save:function(_9){
var _a={"Content-Type":"text/html"};
if(this.url){
var _b={url:this.url,postData:_9,headers:_a,handleAs:"text"};
this.button.set("disabled",true);
var _c=_1.xhrPost(_b);
_c.addCallback(_1.hitch(this,this.onSuccess));
_c.addErrback(_1.hitch(this,this.onError));
}else{
}
},onSuccess:function(_d,_e){
this.button.set("disabled",false);
if(this.logResults){
}
},onError:function(_f,_10){
this.button.set("disabled",false);
if(this.logResults){
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _11=o.args.name.toLowerCase();
if(_11==="save"){
o.plugin=new _5({url:("url" in o.args)?o.args.url:"",logResults:("logResults" in o.args)?o.args.logResults:true});
}
});
return _5;
});
