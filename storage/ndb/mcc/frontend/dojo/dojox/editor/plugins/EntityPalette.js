//>>built
define("dojox/editor/plugins/EntityPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/latinEntities"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins.EntityPalette");
_1.declare("dojox.editor.plugins.EntityPalette",[_4,_5,_6],{templateString:"<div class=\"dojoxEntityPalette\">\n"+"\t<table>\n"+"\t\t<tbody>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table class=\"dijitPaletteTable\">\n"+"\t\t\t\t\t\t<tbody dojoAttachPoint=\"gridNode\"></tbody>\n"+"\t\t\t\t   </table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table dojoAttachPoint=\"previewPane\" class=\"dojoxEntityPalettePreviewTable\">\n"+"\t\t\t\t\t\t<tbody>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Preview</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"codeHeader\">Code</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"entityHeader\">Name</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Description</th>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetailEntity\" dojoAttachPoint=\"previewNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"codeNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"entityNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"descNode\"></td>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t</tbody>\n"+"\t\t\t\t\t</table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t</tbody>\n"+"\t</table>\n"+"</div>",baseClass:"dojoxEntityPalette",showPreview:true,showCode:false,showEntityName:false,palette:"latin",dyeClass:"dojox.editor.plugins.LatinEntity",paletteClass:"editorLatinEntityPalette",cellClass:"dojoxEntityPaletteCell",postMixInProperties:function(){
var _7=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
var _8=0;
var _9;
for(_9 in _7){
_8++;
}
var _a=Math.floor(Math.sqrt(_8));
var _b=_a;
var _c=0;
var _d=[];
var _e=[];
for(_9 in _7){
_c++;
_e.push(_9);
if(_c%_b===0){
_d.push(_e);
_e=[];
}
}
if(_e.length>0){
_d.push(_e);
}
this._palette=_d;
},buildRendering:function(){
this.inherited(arguments);
var _f=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this._preparePalette(this._palette,_f);
var _10=_1.query(".dojoxEntityPaletteCell",this.gridNode);
_1.forEach(_10,function(_11){
this.connect(_11,"onmouseenter","_onCellMouseEnter");
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
},_setCurrent:function(_12){
this.inherited(arguments);
if(this.showPreview){
this._displayDetails(_12);
}
},_displayDetails:function(_13){
var dye=this._getDye(_13);
if(dye){
var _14=dye.getValue();
var _15=dye._alias;
this.previewNode.innerHTML=_14;
this.codeNode.innerHTML="&amp;#"+parseInt(_14.charCodeAt(0),10)+";";
this.entityNode.innerHTML="&amp;"+_15+";";
var _16=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this.descNode.innerHTML=_16[_15].replace("\n","<br>");
}else{
this.previewNode.innerHTML="";
this.codeNode.innerHTML="";
this.entityNode.innerHTML="";
this.descNode.innerHTML="";
}
}});
_1.declare("dojox.editor.plugins.LatinEntity",null,{constructor:function(_17){
this._alias=_17;
},getValue:function(){
return "&"+this._alias+";";
},fillCell:function(_18){
_18.innerHTML=this.getValue();
}});
return _3.editor.plugins.EntityPalette;
});
