//>>built
define("dojox/editor/plugins/PasteFromWord",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/_base/manager","dijit/_editor/RichText","dijit/form/Button","dijit/Dialog","dojox/html/format","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/PasteFromWord","dojo/i18n!dijit/nls/common","dojo/i18n!dijit/_editor/nls/commands"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.PasteFromWord",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",width:"400px",height:"300px",_template:["<div class='dijitPasteFromWordEmbeddedRTE'>","<div style='width: ${width}; padding-top: 5px; padding-bottom: 5px;'>${instructions}</div>","<div id='${uId}_rte' style='width: ${width}; height: ${height}'></div>","<table style='width: ${width}' tabindex='-1'>","<tbody>","<tr>","<td align='center'>","<button type='button' dojoType='dijit.form.Button' id='${uId}_paste'>${paste}</button>","&nbsp;","<button type='button' dojoType='dijit.form.Button' id='${uId}_cancel'>${buttonCancel}</button>","</td>","</tr>","</tbody>","</table>","</div>"].join(""),_filters:[{regexp:/(<meta\s*[^>]*\s*>)|(<\s*link\s* href="file:[^>]*\s*>)|(<\/?\s*\w+:[^>]*\s*>)/gi,handler:""},{regexp:/(?:<style([^>]*)>([\s\S]*?)<\/style>|<link\s+(?=[^>]*rel=['"]?stylesheet)([^>]*?href=(['"])([^>]*?)\4[^>\/]*)\/?>)/gi,handler:""},{regexp:/(class="Mso[^"]*")|(<!--(.|\s){1,}?-->)/gi,handler:""},{regexp:/(<p[^>]*>\s*(\&nbsp;|\u00A0)*\s*<\/p[^>]*>)|(<p[^>]*>\s*<font[^>]*>\s*(\&nbsp;|\u00A0)*\s*<\/\s*font\s*>\s<\/p[^>]*>)/ig,handler:""},{regexp:/(style="[^"]*mso-[^;][^"]*")|(style="margin:\s*[^;"]*;")/gi,handler:""},{regexp:/(<\s*script[^>]*>((.|\s)*?)<\\?\/\s*script\s*>)|(<\s*script\b([^<>]|\s)*>?)|(<[^>]*=(\s|)*[("|')]javascript:[^$1][(\s|.)]*[$1][^>]*>)/ig,handler:""},{regexp:/<(\/?)o\:p[^>]*>/gi,handler:""}],_initButton:function(){
this._filters=this._filters.slice(0);
var _6=_1.i18n.getLocalization("dojox.editor.plugins","PasteFromWord");
_1.mixin(_6,_1.i18n.getLocalization("dijit","common"));
_1.mixin(_6,_1.i18n.getLocalization("dijit._editor","commands"));
this.button=new _2.form.Button({label:_6["pasteFromWord"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"PasteFromWord",tabIndex:"-1",onClick:_1.hitch(this,"_openDialog")});
this._uId=_2.getUniqueId(this.editor.id);
_6.uId=this._uId;
_6.width=this.width||"400px";
_6.height=this.height||"300px";
this._dialog=new _2.Dialog({title:_6["pasteFromWord"]}).placeAt(_1.body());
this._dialog.set("content",_1.string.substitute(this._template,_6));
_1.style(_1.byId(this._uId+"_rte"),"opacity",0.001);
this.connect(_2.byId(this._uId+"_paste"),"onClick","_paste");
this.connect(_2.byId(this._uId+"_cancel"),"onClick","_cancel");
this.connect(this._dialog,"onHide","_clearDialog");
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
},_openDialog:function(){
this._dialog.show();
if(!this._rte){
setTimeout(_1.hitch(this,function(){
this._rte=new _2._editor.RichText({height:this.height||"300px"},this._uId+"_rte");
this._rte.startup();
this._rte.onLoadDeferred.addCallback(_1.hitch(this,function(){
_1.animateProperty({node:this._rte.domNode,properties:{opacity:{start:0.001,end:1}}}).play();
}));
}),100);
}
},_paste:function(){
var _8=_3.html.format.prettyPrint(this._rte.get("value"));
this._dialog.hide();
var i;
for(i=0;i<this._filters.length;i++){
var _9=this._filters[i];
_8=_8.replace(_9.regexp,_9.handler);
}
_8=_3.html.format.prettyPrint(_8);
this.editor.focus();
this.editor.execCommand("inserthtml",_8);
},_cancel:function(){
this._dialog.hide();
},_clearDialog:function(){
this._rte.set("value","");
},destroy:function(){
if(this._rte){
this._rte.destroy();
}
if(this._dialog){
this._dialog.destroyRecursive();
}
delete this._dialog;
delete this._rte;
this.inherited(arguments);
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _a=o.args.name.toLowerCase();
if(_a==="pastefromword"){
o.plugin=new _5({width:("width" in o.args)?o.args.width:"400px",height:("height" in o.args)?o.args.width:"300px"});
}
});
return _5;
});
