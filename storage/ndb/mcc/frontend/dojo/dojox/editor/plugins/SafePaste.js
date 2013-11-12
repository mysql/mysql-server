//>>built
define("dojox/editor/plugins/SafePaste",["dojo","dijit","dojox","dijit/Dialog","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojox/editor/plugins/PasteFromWord","dojo/i18n!dojox/editor/plugins/nls/SafePaste","dojo/i18n!dijit/nls/common","dojo/i18n!dijit/_editor/nls/commands"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.SafePaste",[_3.editor.plugins.PasteFromWord],{_initButton:function(){
this._filters=this._filters.slice(0);
var _4=_1.i18n.getLocalization("dojox.editor.plugins","SafePaste");
_1.mixin(_4,_1.i18n.getLocalization("dijit","common"));
_4.cancel=_4.buttonCancel;
_1.mixin(_4,_1.i18n.getLocalization("dijit._editor","commands"));
this._uId=_2.getUniqueId(this.editor.id);
_4.uId=this._uId;
_4.width=this.width||"400px";
_4.height=this.height||"300px";
this._dialog=new _2.Dialog({title:_4["paste"]}).placeAt(_1.body());
this._dialog.set("content",_1.string.substitute(this._template,_4));
_1.style(_1.byId(this._uId+"_rte"),"opacity",0.001);
this.connect(_2.byId(this._uId+"_paste"),"onClick","_paste");
this.connect(_2.byId(this._uId+"_cancel"),"onClick","_cancel");
this.connect(this._dialog,"onHide","_clearDialog");
_1.forEach(this.stripTags,function(_5){
var _6=_5+"";
var _7=new RegExp("<\\s*"+_6+"[^>]*>","igm");
var _8=new RegExp("<\\\\?\\/\\s*"+_6+"\\s*>","igm");
this._filters.push({regexp:_7,handler:""});
this._filters.push({regexp:_8,handler:""});
},this);
},updateState:function(){
},setEditor:function(_9){
this.editor=_9;
this._initButton();
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
var _a=_1.hitch(this,function(e){
if(e){
_1.stopEvent(e);
}
this._openDialog();
return true;
});
this.connect(this.editor.editNode,"onpaste",_a);
this.editor._pasteImpl=_a;
}));
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _b=o.args.name.toLowerCase();
if(_b==="safepaste"){
o.plugin=new _3.editor.plugins.SafePaste({width:(o.args.hasOwnProperty("width"))?o.args.width:"400px",height:(o.args.hasOwnProperty("height"))?o.args.width:"300px",stripTags:(o.args.hasOwnProperty("stripTags"))?o.args.stripTags:null});
}
});
return _3.editor.plugins.SafePaste;
});
