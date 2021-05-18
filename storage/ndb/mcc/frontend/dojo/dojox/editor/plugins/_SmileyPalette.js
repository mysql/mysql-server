//>>built
define("dojox/editor/plugins/_SmileyPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Smiley"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins._SmileyPalette");
var _7=_1.declare("dojox.editor.plugins.Emoticon",null,{constructor:function(id){
this.id=id;
},getValue:function(){
return _7.ascii[this.id];
},imgHtml:function(_8){
var _9="emoticon"+this.id.substr(0,1).toUpperCase()+this.id.substr(1),_a=_1.moduleUrl("dojox.editor.plugins","resources/emoticons/"+_9+".gif"),_b=_1.i18n.getLocalization("dojox.editor.plugins","Smiley")[_9],_c=["<img src=\"",_a,"\" class=\"",_8,"\" alt=\"",this.getValue(),"\" title=\"",_b,"\">"];
return _c.join("");
},fillCell:function(_d,_e){
_1.place(this.imgHtml("dijitPaletteImg"),_d);
}});
_7.ascii={smile:":-)",laughing:"lol",wink:";-)",grin:":-D",cool:"8-)",angry:":-@",half:":-/",eyebrow:"/:)",frown:":-(",shy:":-$",goofy:":-S",oops:":-O",tongue:":-P",idea:"(i)",yes:"(y)",no:"(n)",angel:"0:-)",crying:":'(",happy:"=)"};
_7.fromAscii=function(_f){
var _10=_7.ascii;
for(var i in _10){
if(_f==_10[i]){
return new _7(i);
}
}
return null;
};
var _11=_1.declare("dojox.editor.plugins._SmileyPalette",[_4,_5,_6],{templateString:"<table class=\"dijitInline dijitEditorSmileyPalette dijitPaletteTable\""+" cellSpacing=0 cellPadding=0><tbody dojoAttachPoint=\"gridNode\"></tbody></table>",baseClass:"dijitEditorSmileyPalette",_palette:[["smile","laughing","wink","grin"],["cool","angry","half","eyebrow"],["frown","shy","goofy","oops"],["tongue","idea","angel","happy"],["yes","no","crying",""]],dyeClass:_7,buildRendering:function(){
this.inherited(arguments);
var _12=_1.i18n.getLocalization("dojox.editor.plugins","Smiley");
var _13={};
for(var _14 in _12){
if(_14.substr(0,8)=="emoticon"){
_13[_14.substr(8).toLowerCase()]=_12[_14];
}
}
this._preparePalette(this._palette,_13);
}});
_11.Emoticon=_7;
return _11;
});
