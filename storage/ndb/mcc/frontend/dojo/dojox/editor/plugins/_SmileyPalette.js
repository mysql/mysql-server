//>>built
define("dojox/editor/plugins/_SmileyPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins._SmileyPalette");
_1.declare("dojox.editor.plugins._SmileyPalette",[_2._Widget,_2._TemplatedMixin,_2._PaletteMixin],{templateString:"<table class=\"dijitInline dijitEditorSmileyPalette dijitPaletteTable\""+" cellSpacing=0 cellPadding=0><tbody dojoAttachPoint=\"gridNode\"></tbody></table>",baseClass:"dijitEditorSmileyPalette",_palette:[["smile","laughing","wink","grin"],["cool","angry","half","eyebrow"],["frown","shy","goofy","oops"],["tongue","idea","angel","happy"],["yes","no","crying",""]],dyeClass:"dojox.editor.plugins.Emoticon",buildRendering:function(){
this.inherited(arguments);
var _4=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
var _5={};
for(var _6 in _4){
if(_6.substr(0,8)=="emoticon"){
_5[_6.substr(8).toLowerCase()]=_4[_6];
}
}
this._preparePalette(this._palette,_5);
}});
_1.declare("dojox.editor.plugins.Emoticon",null,{constructor:function(id){
this.id=id;
},getValue:function(){
return _3.editor.plugins.Emoticon.ascii[this.id];
},imgHtml:function(_7){
var _8="emoticon"+this.id.substr(0,1).toUpperCase()+this.id.substr(1),_9=_1.moduleUrl("dojox.editor.plugins","resources/emoticons/"+_8+".gif"),_a=_1.i18n.getLocalization("dojox.editor.plugins","Smiley")[_8],_b=["<img src=\"",_9,"\" class=\"",_7,"\" alt=\"",this.getValue(),"\" title=\"",_a,"\">"];
return _b.join("");
},fillCell:function(_c,_d){
_1.place(this.imgHtml("dijitPaletteImg"),_c);
}});
_3.editor.plugins.Emoticon.ascii={smile:":-)",laughing:"lol",wink:";-)",grin:":-D",cool:"8-)",angry:":-@",half:":-/",eyebrow:"/:)",frown:":-(",shy:":-$",goofy:":-S",oops:":-O",tongue:":-P",idea:"(i)",yes:"(y)",no:"(n)",angel:"0:-)",crying:":'(",happy:"=)"};
_3.editor.plugins.Emoticon.fromAscii=function(_e){
var _f=_3.editor.plugins.Emoticon.ascii;
for(var i in _f){
if(_e==_f[i]){
return new _3.editor.plugins.Emoticon(i);
}
}
return null;
};
return _3.editor.plugins._SmileyPalette;
});
