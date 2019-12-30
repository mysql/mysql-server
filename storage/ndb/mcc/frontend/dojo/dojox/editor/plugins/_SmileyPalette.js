//>>built
define("dojox/editor/plugins/_SmileyPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins._SmileyPalette");
_1.declare("dojox.editor.plugins._SmileyPalette",[_4,_5,_6],{templateString:"<table class=\"dijitInline dijitEditorSmileyPalette dijitPaletteTable\""+" cellSpacing=0 cellPadding=0><tbody dojoAttachPoint=\"gridNode\"></tbody></table>",baseClass:"dijitEditorSmileyPalette",_palette:[["smile","laughing","wink","grin"],["cool","angry","half","eyebrow"],["frown","shy","goofy","oops"],["tongue","idea","angel","happy"],["yes","no","crying",""]],dyeClass:"dojox.editor.plugins.Emoticon",buildRendering:function(){
this.inherited(arguments);
var _7=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
var _8={};
for(var _9 in _7){
if(_9.substr(0,8)=="emoticon"){
_8[_9.substr(8).toLowerCase()]=_7[_9];
}
}
this._preparePalette(this._palette,_8);
}});
_1.declare("dojox.editor.plugins.Emoticon",null,{constructor:function(id){
this.id=id;
},getValue:function(){
return _3.editor.plugins.Emoticon.ascii[this.id];
},imgHtml:function(_a){
var _b="emoticon"+this.id.substr(0,1).toUpperCase()+this.id.substr(1),_c=_1.moduleUrl("dojox.editor.plugins","resources/emoticons/"+_b+".gif"),_d=_1.i18n.getLocalization("dojox.editor.plugins","Smiley")[_b],_e=["<img src=\"",_c,"\" class=\"",_a,"\" alt=\"",this.getValue(),"\" title=\"",_d,"\">"];
return _e.join("");
},fillCell:function(_f,_10){
_1.place(this.imgHtml("dijitPaletteImg"),_f);
}});
_3.editor.plugins.Emoticon.ascii={smile:":-)",laughing:"lol",wink:";-)",grin:":-D",cool:"8-)",angry:":-@",half:":-/",eyebrow:"/:)",frown:":-(",shy:":-$",goofy:":-S",oops:":-O",tongue:":-P",idea:"(i)",yes:"(y)",no:"(n)",angel:"0:-)",crying:":'(",happy:"=)"};
_3.editor.plugins.Emoticon.fromAscii=function(str){
var _11=_3.editor.plugins.Emoticon.ascii;
for(var i in _11){
if(str==_11[i]){
return new _3.editor.plugins.Emoticon(i);
}
}
return null;
};
return _3.editor.plugins._SmileyPalette;
});
