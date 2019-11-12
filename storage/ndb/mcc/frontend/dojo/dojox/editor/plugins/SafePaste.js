//>>built
define("dojox/editor/plugins/SafePaste",["dojo","dijit","dojox","dojox/editor/plugins/PasteFromWord","dijit/Dialog","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/SafePaste","dojo/i18n!dijit/nls/common","dojo/i18n!dijit/_editor/nls/commands"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.SafePaste",[_4],{_initButton:function(){
this._filters=this._filters.slice(0);
var _5=_1.i18n.getLocalization("dojox.editor.plugins","SafePaste");
_1.mixin(_5,_1.i18n.getLocalization("dijit","common"));
_1.mixin(_5,_1.i18n.getLocalization("dijit._editor","commands"));
this._uId=_2.getUniqueId(this.editor.id);
_5.uId=this._uId;
_5.width=this.width||"400px";
_5.height=this.height||"300px";
this._dialog=new _2.Dialog({title:_5["paste"]}).placeAt(_1.body());
this._dialog.set("content",_1.string.substitute(this._template,_5));
_1.style(_1.byId(this._uId+"_rte"),"opacity",0.001);
this.connect(_2.byId(this._uId+"_paste"),"onClick","_paste");
this.connect(_2.byId(this._uId+"_cancel"),"onClick","_cancel");
this.connect(this._dialog,"onHide","_clearDialog");
_1.forEach(this.stripTags,function(_6){
var _7=_6+"";
var _8=new RegExp("<\\s*"+_7+"[^>]*>","igm");
var _9=new RegExp("<\\\\?\\/\\s*"+_7+"\\s*>","igm");
this._filters.push({regexp:_8,handler:""});
this._filters.push({regexp:_9,handler:""});
},this);
},updateState:function(){
},setEditor:function(_a){
this.editor=_a;
this._initButton();
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
var _b=_1.hitch(this,function(e){
if(e){
_1.stopEvent(e);
}
this._openDialog();
return true;
});
this.connect(this.editor.editNode,"onpaste",_b);
this.editor._pasteImpl=_b;
}));
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _c=o.args.name.toLowerCase();
if(_c==="safepaste"){
o.plugin=new _3.editor.plugins.SafePaste({width:(o.args.hasOwnProperty("width"))?o.args.width:"400px",height:(o.args.hasOwnProperty("height"))?o.args.width:"300px",stripTags:(o.args.hasOwnProperty("stripTags"))?o.args.stripTags:null});
}
});
return _3.editor.plugins.SafePaste;
});
