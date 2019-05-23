//>>built
define("dojox/drawing/plugins/drawing/GreekPalette",["dojo","dijit/popup","../../library/greek","dijit/focus","dijit/_Widget","dijit/_TemplatedMixin","dijit/_PaletteMixin","dojo/i18n!dojox/editor/plugins/nls/latinEntities"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1.declare(null,{constructor:function(_a){
this._alias=_a;
},getValue:function(){
return this._alias;
},fillCell:function(_b){
_b.innerHTML="&"+this._alias+";";
}});
return _1.declare("dojox.drawing.plugins.drawing.GreekPalette",[_5,_6,_7],{postMixInProperties:function(){
var _c=_3;
var _d=0;
var _e;
for(_e in _c){
_d++;
}
var _f=Math.floor(Math.sqrt(_d));
var _10=_f;
var _11=0;
var _12=[];
var row=[];
for(_e in _c){
_11++;
row.push(_e);
if(_11%_10===0){
_12.push(row);
row=[];
}
}
if(row.length>0){
_12.push(row);
}
this._palette=_12;
},show:function(obj){
_1.mixin(obj,{popup:this});
_2.open(obj);
},onChange:function(val){
var _13=this._textBlock;
_2.hide(this);
_13.insertText(this._pushChangeTo,val);
_13._dropMode=false;
},onCancel:function(_14){
_2.hide(this);
this._textBlock._dropMode=false;
},templateString:"<div class=\"dojoxEntityPalette\">\n"+"\t<table>\n"+"\t\t<tbody>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table class=\"dijitPaletteTable\">\n"+"\t\t\t\t\t\t<tbody dojoAttachPoint=\"gridNode\"></tbody>\n"+"\t\t\t\t   </table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t\t<tr>\n"+"\t\t\t\t<td>\n"+"\t\t\t\t\t<table dojoAttachPoint=\"previewPane\" class=\"dojoxEntityPalettePreviewTable\">\n"+"\t\t\t\t\t\t<tbody>\n"+"\t\t\t\t\t\t\t<tr>\n"+"\t\t\t\t\t\t\t\t<td class=\"dojoxEntityPalettePreviewDetailEntity\">Type: <span class=\"dojoxEntityPalettePreviewDetail\" dojoAttachPoint=\"previewNode\"></span></td>\n"+"\t\t\t\t\t\t\t</tr>\n"+"\t\t\t\t\t\t</tbody>\n"+"\t\t\t\t\t</table>\n"+"\t\t\t\t</td>\n"+"\t\t\t</tr>\n"+"\t\t</tbody>\n"+"\t</table>\n"+"</div>",baseClass:"dojoxEntityPalette",showPreview:true,dyeClass:_9,paletteClass:"editorLatinEntityPalette",cellClass:"dojoxEntityPaletteCell",buildRendering:function(){
this.inherited(arguments);
var _15=_8;
this._preparePalette(this._palette,_15);
var _16=_1.query(".dojoxEntityPaletteCell",this.gridNode);
_1.forEach(_16,function(_17){
this.connect(_17,"onmouseenter","_onCellMouseEnter");
},this);
},_onCellMouseEnter:function(e){
if(this.showPreview){
this._displayDetails(e.target);
}
},_onCellClick:function(evt){
var _18=evt.type=="click"?evt.currentTarget:this._currentFocus,_19=this._getDye(_18).getValue();
this._setCurrent(_18);
setTimeout(_1.hitch(this,function(){
_4(_18);
this._setValueAttr(_19,true);
}));
_1.removeClass(_18,"dijitPaletteCellHover");
_1.stopEvent(evt);
},postCreate:function(){
this.inherited(arguments);
if(!this.showPreview){
_1.style(this.previewNode,"display","none");
}
_2.moveOffScreen(this);
},_setCurrent:function(_1a){
if("_currentFocus" in this){
_1.attr(this._currentFocus,"tabIndex","-1");
_1.removeClass(this._currentFocus,"dojoxEntityPaletteCellHover");
}
this._currentFocus=_1a;
if(_1a){
_1.attr(_1a,"tabIndex",this.tabIndex);
_1.addClass(this._currentFocus,"dojoxEntityPaletteCellHover");
}
if(this.showPreview){
this._displayDetails(_1a);
}
},_displayDetails:function(_1b){
var dye=this._getDye(_1b);
if(dye){
var _1c=dye.getValue();
var _1d=dye._alias;
this.previewNode.innerHTML=_1c;
}else{
this.previewNode.innerHTML="";
this.descNode.innerHTML="";
}
},_preparePalette:function(_1e,_1f){
this._cells=[];
var url=this._blankGif;
var _20=typeof this.dyeClass==="string"?_1.getObject(this.dyeClass):this.dyeClass;
for(var row=0;row<_1e.length;row++){
var _21=_1.create("tr",{tabIndex:"-1"},this.gridNode);
for(var col=0;col<_1e[row].length;col++){
var _22=_1e[row][col];
if(_22){
var _23=new _20(_22);
var _24=_1.create("td",{"class":this.cellClass,tabIndex:"-1",title:_1f[_22]});
_23.fillCell(_24,url);
this.connect(_24,"ondijitclick","_onCellClick");
this._trackMouseState(_24,this.cellClass);
_1.place(_24,_21);
_24.idx=this._cells.length;
this._cells.push({node:_24,dye:_23});
}
}
}
this._xDim=_1e[0].length;
this._yDim=_1e.length;
},_navigateByArrow:function(evt){
var _25={38:-this._xDim,40:this._xDim,39:this.isLeftToRight()?1:-1,37:this.isLeftToRight()?-1:1};
var _26=_25[evt.keyCode];
var _27=this._currentFocus.idx+_26;
if(_27<this._cells.length&&_27>-1){
var _28=this._cells[_27].node;
this._setCurrent(_28);
}
}});
});
