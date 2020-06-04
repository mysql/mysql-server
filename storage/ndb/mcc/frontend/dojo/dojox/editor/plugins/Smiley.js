//>>built
define("dojox/editor/plugins/Smiley",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/DropDownButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojox/editor/plugins/_SmileyPalette","dojox/html/format","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3,_4){
_1.experimental("dojox.editor.plugins.Smiley");
var _5=_1.declare("dojox.editor.plugins.Smiley",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",emoticonMarker:"[]",emoticonImageClass:"dojoEditorEmoticon",_initButton:function(){
this.dropDown=new _3.editor.plugins._SmileyPalette();
this.connect(this.dropDown,"onChange",function(_6){
this.button.closeDropDown();
this.editor.focus();
_6=this.emoticonMarker.charAt(0)+_6+this.emoticonMarker.charAt(1);
this.editor.execCommand("inserthtml",_6);
});
this.i18n=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
this.button=new _2.form.DropDownButton({label:this.i18n.smiley,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Smiley",tabIndex:"-1",dropDown:this.dropDown});
this.emoticonImageRegexp=new RegExp("class=(\"|')"+this.emoticonImageClass+"(\"|')");
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
this.editor.contentPreFilters.push(_1.hitch(this,this._preFilterEntities));
this.editor.contentPostFilters.push(_1.hitch(this,this._postFilterEntities));
if(_1.isFF){
var _8=_1.hitch(this,function(){
var _9=this.editor;
setTimeout(function(){
if(_9.editNode){
_1.style(_9.editNode,"opacity","0.99");
setTimeout(function(){
if(_9.editNode){
_1.style(_9.editNode,"opacity","");
}
},0);
}
},0);
return true;
});
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.editor.addKeyHandler(_1.keys.DELETE,false,false,_8);
this.editor.addKeyHandler(_1.keys.BACKSPACE,false,false,_8);
}));
}
},_preFilterEntities:function(_a){
return _a.replace(/\[([^\]]*)\]/g,_1.hitch(this,this._decode));
},_postFilterEntities:function(_b){
return _b.replace(/<img [^>]*>/gi,_1.hitch(this,this._encode));
},_decode:function(_c,_d){
var _e=_3.editor.plugins.Emoticon.fromAscii(_d);
return _e?_e.imgHtml(this.emoticonImageClass):_c;
},_encode:function(_f){
if(_f.search(this.emoticonImageRegexp)>-1){
return this.emoticonMarker.charAt(0)+_f.replace(/(<img [^>]*)alt="([^"]*)"([^>]*>)/,"$2")+this.emoticonMarker.charAt(1);
}else{
return _f;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
if(o.args.name==="smiley"){
o.plugin=new _5();
}
});
return _5;
});
