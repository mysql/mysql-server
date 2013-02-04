//>>built
define("dojox/editor/plugins/EntityPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/latinEntities"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.EntityPalette");
_1.declare("dojox.editor.plugins.EntityPalette",[_2._Widget,_2._TemplatedMixin,_2._PaletteMixin],{templateString:"<div class=\"dojoxEntityPalette\">\n"+"\t<table>\n"+"\t\t<tbody>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table class=\"dijitPaletteTable\">\n"+"\t\t\t\t\t\t<tbody dojoAttachPoint=\"gridNode\"></tbody>\n"+"\t\t\t\t   </table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table dojoAttachPoint=\"previewPane\" class=\"dojoxEntityPalettePreviewTable\">\n"+"\t\t\t\t\t\t<tbody>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Preview</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"codeHeader\">Code</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"entityHeader\">Name</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Description</th>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetailEntity\" dojoAttachPoint=\"previewNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"codeNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"entityNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"descNode\"></td>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t</tbody>\n"+"\t\t\t\t\t</table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t</tbody>\n"+"\t</table>\n"+"</div>",baseClass:"dojoxEntityPalette",showPreview:true,showCode:false,showEntityName:false,palette:"latin",dyeClass:"dojox.editor.plugins.LatinEntity",paletteClass:"editorLatinEntityPalette",cellClass:"dojoxEntityPaletteCell",postMixInProperties:function(){
var _4=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
var _5=0;
var _6;
for(_6 in _4){
_5++;
}
var _7=Math.floor(Math.sqrt(_5));
var _8=_7;
var _9=0;
var _a=[];
var _b=[];
for(_6 in _4){
_9++;
_b.push(_6);
if(_9%_8===0){
_a.push(_b);
_b=[];
}
}
if(_b.length>0){
_a.push(_b);
}
this._palette=_a;
},buildRendering:function(){
this.inherited(arguments);
var _c=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this._preparePalette(this._palette,_c);
var _d=_1.query(".dojoxEntityPaletteCell",this.gridNode);
_1.forEach(_d,function(_e){
this.connect(_e,"onmouseenter","_onCellMouseEnter");
},this);
},_onCellMouseEnter:function(e){
this._displayDetails(e.target);
},postCreate:function(){
this.inherited(arguments);
_1.style(this.codeHeader,"display",this.showCode?"":"none");
_1.style(this.codeNode,"display",this.showCode?"":"none");
_1.style(this.entityHeader,"display",this.showEntityName?"":"none");
_1.style(this.entityNode,"display",this.showEntityName?"":"none");
if(!this.showPreview){
_1.style(this.previewNode,"display","none");
}
},_setCurrent:function(_f){
this.inherited(arguments);
if(this.showPreview){
this._displayDetails(_f);
}
},_displayDetails:function(_10){
var dye=this._getDye(_10);
if(dye){
var _11=dye.getValue();
var _12=dye._alias;
this.previewNode.innerHTML=_11;
this.codeNode.innerHTML="&amp;#"+parseInt(_11.charCodeAt(0),10)+";";
this.entityNode.innerHTML="&amp;"+_12+";";
var _13=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this.descNode.innerHTML=_13[_12].replace("\n","<br>");
}else{
this.previewNode.innerHTML="";
this.codeNode.innerHTML="";
this.entityNode.innerHTML="";
this.descNode.innerHTML="";
}
}});
_1.declare("dojox.editor.plugins.LatinEntity",null,{constructor:function(_14){
this._alias=_14;
},getValue:function(){
return "&"+this._alias+";";
},fillCell:function(_15){
_15.innerHTML=this.getValue();
}});
return _3.editor.plugins.EntityPalette;
});
