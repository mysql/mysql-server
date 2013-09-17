//>>built
require({cache:{"url:dijit/templates/ColorPalette.html":"<div class=\"dijitInline dijitColorPalette\">\n\t<table dojoAttachPoint=\"paletteTableNode\" class=\"dijitPaletteTable\" cellSpacing=\"0\" cellPadding=\"0\" role=\"grid\">\n\t\t<tbody data-dojo-attach-point=\"gridNode\"></tbody>\n\t</table>\n</div>\n"}});
define("dijit/ColorPalette",["require","dojo/text!./templates/ColorPalette.html","./_Widget","./_TemplatedMixin","./_PaletteMixin","dojo/i18n","dojo/_base/Color","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/_base/window","dojo/string","dojo/i18n!dojo/nls/colors","dojo/colors"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_8("dijit.ColorPalette",[_3,_4,_5],{palette:"7x10",_palettes:{"7x10":[["white","seashell","cornsilk","lemonchiffon","lightyellow","palegreen","paleturquoise","lightcyan","lavender","plum"],["lightgray","pink","bisque","moccasin","khaki","lightgreen","lightseagreen","lightskyblue","cornflowerblue","violet"],["silver","lightcoral","sandybrown","orange","palegoldenrod","chartreuse","mediumturquoise","skyblue","mediumslateblue","orchid"],["gray","red","orangered","darkorange","yellow","limegreen","darkseagreen","royalblue","slateblue","mediumorchid"],["dimgray","crimson","chocolate","coral","gold","forestgreen","seagreen","blue","blueviolet","darkorchid"],["darkslategray","firebrick","saddlebrown","sienna","olive","green","darkcyan","mediumblue","darkslateblue","darkmagenta"],["black","darkred","maroon","brown","darkolivegreen","darkgreen","midnightblue","navy","indigo","purple"]],"3x4":[["white","lime","green","blue"],["silver","yellow","fuchsia","navy"],["gray","red","purple","black"]]},templateString:_2,baseClass:"dijitColorPalette",_dyeFactory:function(_e,_f,col){
return new this._dyeClass(_e,_f,col);
},buildRendering:function(){
this.inherited(arguments);
this._dyeClass=_8(_d._Color,{hc:_9.contains(_b.body(),"dijit_a11y"),palette:this.palette});
this._preparePalette(this._palettes[this.palette],_6.getLocalization("dojo","colors",this.lang));
}});
_d._Color=_8("dijit._Color",_7,{template:"<span class='dijitInline dijitPaletteImg'>"+"<img src='${blankGif}' alt='${alt}' class='dijitColorPaletteSwatch' style='background-color: ${color}'/>"+"</span>",hcTemplate:"<span class='dijitInline dijitPaletteImg' style='position: relative; overflow: hidden; height: 12px; width: 14px;'>"+"<img src='${image}' alt='${alt}' style='position: absolute; left: ${left}px; top: ${top}px; ${size}'/>"+"</span>",_imagePaths:{"7x10":_1.toUrl("./themes/a11y/colors7x10.png"),"3x4":_1.toUrl("./themes/a11y/colors3x4.png")},constructor:function(_10,row,col){
this._alias=_10;
this._row=row;
this._col=col;
this.setColor(_7.named[_10]);
},getValue:function(){
return this.toHex();
},fillCell:function(_11,_12){
var _13=_c.substitute(this.hc?this.hcTemplate:this.template,{color:this.toHex(),blankGif:_12,alt:this._alias,image:this._imagePaths[this.palette].toString(),left:this._col*-20-5,top:this._row*-20-5,size:this.palette=="7x10"?"height: 145px; width: 206px":"height: 64px; width: 86px"});
_a.place(_13,_11);
}});
return _d;
});
