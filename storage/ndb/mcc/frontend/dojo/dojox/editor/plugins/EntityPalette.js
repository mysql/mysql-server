//>>built
define("dojox/editor/plugins/EntityPalette",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/latinEntities"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins.EntityPalette");
var _7=_1.declare("dojox.editor.plugins.EntityPalette",[_4,_5,_6],{templateString:"<div class=\"dojoxEntityPalette\">\n"+"\t<table>\n"+"\t\t<tbody>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table class=\"dijitPaletteTable\">\n"+"\t\t\t\t\t\t<tbody dojoAttachPoint=\"gridNode\"></tbody>\n"+"\t\t\t\t   </table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table dojoAttachPoint=\"previewPane\" class=\"dojoxEntityPalettePreviewTable\">\n"+"\t\t\t\t\t\t<tbody>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Preview</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"codeHeader\">Code</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\" dojoAttachPoint=\"entityHeader\">Name</th>\n"+"\t\t\t\t\t\t\t\t<th class=\"dojoxEntityPalettePreviewHeader\">Description</th>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetailEntity\" dojoAttachPoint=\"previewNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"codeNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"entityNode\"></td>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"descNode\"></td>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t</tbody>\n"+"\t\t\t\t\t</table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t</tbody>\n"+"\t</table>\n"+"</div>",baseClass:"dojoxEntityPalette",showPreview:true,showCode:false,showEntityName:false,palette:"latin",dyeClass:"dojox.editor.plugins.LatinEntity",paletteClass:"editorLatinEntityPalette",cellClass:"dojoxEntityPaletteCell",postMixInProperties:function(){
var _8=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
var _9=0;
var _a;
for(_a in _8){
_9++;
}
var _b=Math.floor(Math.sqrt(_9));
var _c=_b;
var _d=0;
var _e=[];
var _f=[];
for(_a in _8){
_d++;
_f.push(_a);
if(_d%_c===0){
_e.push(_f);
_f=[];
}
}
if(_f.length>0){
_e.push(_f);
}
this._palette=_e;
},buildRendering:function(){
this.inherited(arguments);
var _10=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this._preparePalette(this._palette,_10);
var _11=_1.query(".dojoxEntityPaletteCell",this.gridNode);
_1.forEach(_11,function(_12){
this.connect(_12,"onmouseenter","_onCellMouseEnter");
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
},_setCurrent:function(_13){
this.inherited(arguments);
if(this.showPreview){
this._displayDetails(_13);
}
},_displayDetails:function(_14){
var dye=this._getDye(_14);
if(dye){
var _15=dye.getValue();
var _16=dye._alias;
this.previewNode.innerHTML=_15;
this.codeNode.innerHTML="&amp;#"+parseInt(_15.charCodeAt(0),10)+";";
this.entityNode.innerHTML="&amp;"+_16+";";
var _17=_1.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this.descNode.innerHTML=_17[_16].replace("\n","<br>");
}else{
this.previewNode.innerHTML="";
this.codeNode.innerHTML="";
this.entityNode.innerHTML="";
this.descNode.innerHTML="";
}
}});
_7.LatinEntity=_1.declare("dojox.editor.plugins.LatinEntity",null,{constructor:function(_18){
this._alias=_18;
},getValue:function(){
return "&"+this._alias+";";
},fillCell:function(_19){
_19.innerHTML=this.getValue();
}});
return _7;
});
