//>>built
define("dojox/editor/plugins/Smiley",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/DropDownButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojox/editor/plugins/_SmileyPalette","dojox/html/format","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.Smiley");
_1.declare("dojox.editor.plugins.Smiley",_2._editor._Plugin,{iconClassPrefix:"dijitAdditionalEditorIcon",emoticonMarker:"[]",emoticonImageClass:"dojoEditorEmoticon",_initButton:function(){
this.dropDown=new _3.editor.plugins._SmileyPalette();
this.connect(this.dropDown,"onChange",function(_4){
this.button.closeDropDown();
this.editor.focus();
_4=this.emoticonMarker.charAt(0)+_4+this.emoticonMarker.charAt(1);
this.editor.execCommand("inserthtml",_4);
});
this.i18n=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
this.button=new _2.form.DropDownButton({label:this.i18n.smiley,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Smiley",tabIndex:"-1",dropDown:this.dropDown});
this.emoticonImageRegexp=new RegExp("class=(\"|')"+this.emoticonImageClass+"(\"|')");
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_5){
this.editor=_5;
this._initButton();
this.editor.contentPreFilters.push(_1.hitch(this,this._preFilterEntities));
this.editor.contentPostFilters.push(_1.hitch(this,this._postFilterEntities));
if(_1.isFF){
var _6=_1.hitch(this,function(){
var _7=this.editor;
setTimeout(function(){
if(_7.editNode){
_1.style(_7.editNode,"opacity","0.99");
setTimeout(function(){
if(_7.editNode){
_1.style(_7.editNode,"opacity","");
}
},0);
}
},0);
return true;
});
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.editor.addKeyHandler(_1.keys.DELETE,false,false,_6);
this.editor.addKeyHandler(_1.keys.BACKSPACE,false,false,_6);
}));
}
},_preFilterEntities:function(_8){
return _8.replace(/\[([^\]]*)\]/g,_1.hitch(this,this._decode));
},_postFilterEntities:function(_9){
return _9.replace(/<img [^>]*>/gi,_1.hitch(this,this._encode));
},_decode:function(_a,_b){
var _c=_3.editor.plugins.Emoticon.fromAscii(_b);
return _c?_c.imgHtml(this.emoticonImageClass):_a;
},_encode:function(_d){
if(_d.search(this.emoticonImageRegexp)>-1){
return this.emoticonMarker.charAt(0)+_d.replace(/(<img [^>]*)alt="([^"]*)"([^>]*>)/,"$2")+this.emoticonMarker.charAt(1);
}else{
return _d;
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
