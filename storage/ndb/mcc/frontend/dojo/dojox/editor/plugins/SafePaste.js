//>>built
define("dojox/editor/plugins/SafePaste",["dojo","dijit","dojox","dojox/editor/plugins/PasteFromWord","dijit/Dialog","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/SafePaste","dojo/i18n!dijit/nls/common","dojo/i18n!dijit/_editor/nls/commands"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.SafePaste",[_4],{_initButton:function(){
this._filters=this._filters.slice(0);
var _6=_1.i18n.getLocalization("dojox.editor.plugins","SafePaste");
_1.mixin(_6,_1.i18n.getLocalization("dijit","common"));
_1.mixin(_6,_1.i18n.getLocalization("dijit._editor","commands"));
this._uId=_2.getUniqueId(this.editor.id);
_6.uId=this._uId;
_6.width=this.width||"400px";
_6.height=this.height||"300px";
this._dialog=new _2.Dialog({title:_6["paste"]}).placeAt(_1.body());
this._dialog.set("content",_1.string.substitute(this._template,_6));
_1.style(_1.byId(this._uId+"_rte"),"opacity",0.001);
this.connect(_2.byId(this._uId+"_paste"),"onClick","_paste");
this.connect(_2.byId(this._uId+"_cancel"),"onClick","_cancel");
this.connect(this._dialog,"onHide","_clearDialog");
_1.forEach(this.stripTags,function(_7){
var _8=_7+"";
var _9=new RegExp("<\\s*"+_8+"[^>]*>","igm");
var _a=new RegExp("<\\\\?\\/\\s*"+_8+"\\s*>","igm");
this._filters.push({regexp:_9,handler:""});
this._filters.push({regexp:_a,handler:""});
},this);
},updateState:function(){
},setEditor:function(_b){
this.editor=_b;
this._initButton();
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
var _c=_1.hitch(this,function(e){
if(e){
_1.stopEvent(e);
}
this._openDialog();
return true;
});
this.connect(this.editor.editNode,"onpaste",_c);
this.editor._pasteImpl=_c;
}));
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _d=o.args.name.toLowerCase();
if(_d==="safepaste"){
o.plugin=new _5({width:(o.args.hasOwnProperty("width"))?o.args.width:"400px",height:(o.args.hasOwnProperty("height"))?o.args.width:"300px",stripTags:(o.args.hasOwnProperty("stripTags"))?o.args.stripTags:null});
}
});
return _5;
});
