//>>built
define(["dijit","dojo","dojox","dojo/i18n!dojox/editor/plugins/nls/latinEntities","dojo/require!dojox/drawing/library/greek,dijit/focus,dijit/_Widget,dijit/_TemplatedMixin,dijit/_PaletteMixin,dojo/i18n"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.drawing.GreekPalette");
_2.require("dojox.drawing.library.greek");
_2.require("dijit.focus");
_2.require("dijit._Widget");
_2.require("dijit._TemplatedMixin");
_2.require("dijit._PaletteMixin");
_2.require("dojo.i18n");
_2.requireLocalization("dojox.editor.plugins","latinEntities");
_2.declare("dojox.drawing.plugins.drawing.GreekPalette",[_1._Widget,_1._TemplatedMixin,_1._PaletteMixin],{postMixInProperties:function(){
var _4=_3.drawing.library.greek;
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
},show:function(_c){
_2.mixin(_c,{popup:this});
_1.popup.open(_c);
},onChange:function(_d){
var _e=this._textBlock;
_1.popup.hide(this);
_e.insertText(this._pushChangeTo,_d);
_e._dropMode=false;
},onCancel:function(_f){
_1.popup.hide(this);
this._textBlock._dropMode=false;
},templateString:"<div class=\"dojoxEntityPalette\">\n"+"\t<table>\n"+"\t\t<tbody>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table class=\"dijitPaletteTable\">\n"+"\t\t\t\t\t\t<tbody dojoAttachPoint=\"gridNode\"></tbody>\n"+"\t\t\t\t   </table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table dojoAttachPoint=\"previewPane\" class=\"dojoxEntityPalettePreviewTable\">\n"+"\t\t\t\t\t\t<tbody>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetailEntity\">Type: <span class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"previewNode\"></span></td>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t</tbody>\n"+"\t\t\t\t\t</table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t</tbody>\n"+"\t</table>\n"+"</div>",baseClass:"dojoxEntityPalette",showPreview:true,dyeClass:"dojox.drawing.plugins.Greeks",paletteClass:"editorLatinEntityPalette",cellClass:"dojoxEntityPaletteCell",buildRendering:function(){
this.inherited(arguments);
var _10=_2.i18n.getLocalization("dojox.editor.plugins","latinEntities");
this._preparePalette(this._palette,_10);
var _11=_2.query(".dojoxEntityPaletteCell",this.gridNode);
_2.forEach(_11,function(_12){
this.connect(_12,"onmouseenter","_onCellMouseEnter");
},this);
},_onCellMouseEnter:function(e){
if(this.showPreview){
this._displayDetails(e.target);
}
},_onCellClick:function(evt){
var _13=evt.type=="click"?evt.currentTarget:this._currentFocus,_14=this._getDye(_13).getValue();
this._setCurrent(_13);
setTimeout(_2.hitch(this,function(){
_1.focus(_13);
this._setValueAttr(_14,true);
}));
_2.removeClass(_13,"dijitPaletteCellHover");
_2.stopEvent(evt);
},postCreate:function(){
this.inherited(arguments);
if(!this.showPreview){
_2.style(this.previewNode,"display","none");
}
_1.popup.moveOffScreen(this);
},_setCurrent:function(_15){
if("_currentFocus" in this){
_2.attr(this._currentFocus,"tabIndex","-1");
_2.removeClass(this._currentFocus,"dojoxEntityPaletteCellHover");
}
this._currentFocus=_15;
if(_15){
_2.attr(_15,"tabIndex",this.tabIndex);
_2.addClass(this._currentFocus,"dojoxEntityPaletteCellHover");
}
if(this.showPreview){
this._displayDetails(_15);
}
},_displayDetails:function(_16){
var dye=this._getDye(_16);
if(dye){
var _17=dye.getValue();
var _18=dye._alias;
this.previewNode.innerHTML=_17;
}else{
this.previewNode.innerHTML="";
this.descNode.innerHTML="";
}
},_preparePalette:function(_19,_1a){
this._cells=[];
var url=this._blankGif;
var _1b=_2.getObject(this.dyeClass);
for(var row=0;row<_19.length;row++){
var _1c=_2.create("tr",{tabIndex:"-1"},this.gridNode);
for(var col=0;col<_19[row].length;col++){
var _1d=_19[row][col];
if(_1d){
var _1e=new _1b(_1d);
var _1f=_2.create("td",{"class":this.cellClass,tabIndex:"-1",title:_1a[_1d]});
_1e.fillCell(_1f,url);
this.connect(_1f,"ondijitclick","_onCellClick");
this._trackMouseState(_1f,this.cellClass);
_2.place(_1f,_1c);
_1f.index=this._cells.length;
this._cells.push({node:_1f,dye:_1e});
}
}
}
this._xDim=_19[0].length;
this._yDim=_19.length;
},_navigateByArrow:function(evt){
var _20={38:-this._xDim,40:this._xDim,39:this.isLeftToRight()?1:-1,37:this.isLeftToRight()?-1:1};
var _21=_20[evt.keyCode];
var _22=this._currentFocus.index+_21;
if(_22<this._cells.length&&_22>-1){
var _23=this._cells[_22].node;
this._setCurrent(_23);
}
}});
_2.declare("dojox.drawing.plugins.Greeks",null,{constructor:function(_24){
this._alias=_24;
},getValue:function(){
return this._alias;
},fillCell:function(_25){
_25.innerHTML="&"+this._alias+";";
}});
});
