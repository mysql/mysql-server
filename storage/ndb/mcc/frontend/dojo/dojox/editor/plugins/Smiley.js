//>>built
define("dojox/editor/plugins/Smiley",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/DropDownButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojox/editor/plugins/_SmileyPalette","dojox/html/format","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3,_4){
_1.experimental("dojox.editor.plugins.Smiley");
_1.declare("dojox.editor.plugins.Smiley",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",emoticonMarker:"[]",emoticonImageClass:"dojoEditorEmoticon",_initButton:function(){
this.dropDown=new _3.editor.plugins._SmileyPalette();
this.connect(this.dropDown,"onChange",function(_5){
this.button.closeDropDown();
this.editor.focus();
_5=this.emoticonMarker.charAt(0)+_5+this.emoticonMarker.charAt(1);
this.editor.execCommand("inserthtml",_5);
});
this.i18n=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
this.button=new _2.form.DropDownButton({label:this.i18n.smiley,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Smiley",tabIndex:"-1",dropDown:this.dropDown});
this.emoticonImageRegexp=new RegExp("class=(\"|')"+this.emoticonImageClass+"(\"|')");
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_6){
this.editor=_6;
this._initButton();
this.editor.contentPreFilters.push(_1.hitch(this,this._preFilterEntities));
this.editor.contentPostFilters.push(_1.hitch(this,this._postFilterEntities));
if(_1.isFF){
var _7=_1.hitch(this,function(){
var _8=this.editor;
setTimeout(function(){
if(_8.editNode){
_1.style(_8.editNode,"opacity","0.99");
setTimeout(function(){
if(_8.editNode){
_1.style(_8.editNode,"opacity","");
}
},0);
}
},0);
return true;
});
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.editor.addKeyHandler(_1.keys.DELETE,false,false,_7);
this.editor.addKeyHandler(_1.keys.BACKSPACE,false,false,_7);
}));
}
},_preFilterEntities:function(_9){
return _9.replace(/\[([^\]]*)\]/g,_1.hitch(this,this._decode));
},_postFilterEntities:function(_a){
return _a.replace(/<img [^>]*>/gi,_1.hitch(this,this._encode));
},_decode:function(_b,_c){
var _d=_3.editor.plugins.Emoticon.fromAscii(_c);
return _d?_d.imgHtml(this.emoticonImageClass):_b;
},_encode:function(_e){
if(_e.search(this.emoticonImageRegexp)>-1){
return this.emoticonMarker.charAt(0)+_e.replace(/(<img [^>]*)alt="([^"]*)"([^>]*>)/,"$2")+this.emoticonMarker.charAt(1);
}else{
return _e;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
if(o.args.name==="smiley"){
o.plugin=new _3.editor.plugins.Smiley();
}
});
return _3.editor.plugins.Smiley;
});
