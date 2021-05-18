//>>built
define("dojox/grid/bidi/_BidiMixin",["../../main","dojo/_base/lang","../_Builder","dijit/_BidiSupport","../_Grid","../cells/_base","../cells/dijit"],function(_1,_2,_3,_4,_5,_6,_7){
_2.extend(_5,{setCellNodeTextDirection:function(_8,_9,_a){
this.getCell(_8).getNode(_9).style.direction=_a||"";
},getCellNodeTextDirection:function(_b,_c){
return this.getCell(_b).getNode(_c).style.direction;
},_setTextDirAttr:function(_d){
this.textDir=_d;
this.render();
}});
_2.extend(_3._ContentBuilder,{_getTextDirStyle:function(_e,_f,_10){
var _11=this.grid.getItem(_10),ret="";
if(_e==="auto"){
var _12=_f.get?_f.get(_10,_11):(_f.value||_f.defaultValue);
if(_12){
_e=_4.prototype._checkContextual(_12);
}
}
ret=" direction:"+_e+";";
return ret;
}});
_2.extend(_3._HeaderBuilder,{_getTextDirStyle:function(_13,_14,_15){
if(_13==="auto"){
var _16=_15||_14.name||_14.grid.getCellName(_14);
if(_16){
_13=_4.prototype._checkContextual(_16);
}
}
return (" direction:"+_13+"; ");
}});
_2.extend(_6.Cell,{LRE:"‪",RLE:"‫",PDF:"‬",KEY_HANDLER:"onkeyup=' javascript:(function(){"+"var target; if (event.target) target = event.target; else if (event.srcElement) target = event.srcElement; if(!target) return;"+"var regExMatch = /[A-Za-zא-ٟ٪-ۯۺ-߿יִ-﷿ﹰ-ﻼ]/.exec(target.value);"+"target.dir = regExMatch ? ( regExMatch[0] <= \"z\" ? \"ltr\" : \"rtl\" ) : target.dir ? target.dir : \"ltr\"; })();'",_getTextDirMarkup:function(_17){
var _18="",_19=this.textDir||this.grid.textDir;
if(_19){
if(_19==="auto"){
_18=this.KEY_HANDLER;
_19=_4.prototype._checkContextual(_17);
}
_18+=" dir='"+_19+"'; ";
}
return _18;
},formatEditing:function(_1a,_1b){
this.needFormatNode(_1a,_1b);
return "<input class=\"dojoxGridInput\" "+this._getTextDirMarkup(_1a)+" type=\"text\" value=\""+_1a+"\">";
},_enforceTextDirWithUcc:function(_1c,_1d){
_1c=(_1c==="auto")?_4.prototype._checkContextual(_1d):_1c;
return (_1c==="rtl"?this.RLE:this.LRE)+_1d+this.PDF;
}});
_2.extend(_6.Select,{_getValueCallOrig:_1.grid.cells.Select.prototype.getValue,getValue:function(_1e){
var ret=this._getValueCallOrig(_1e);
if(ret&&(this.textDir||this.grid.textDir)){
ret=ret.replace(/\u202A|\u202B|\u202C/g,"");
}
return ret;
},formatEditing:function(_1f,_20){
this.needFormatNode(_1f,_20);
var h=["<select dir = \""+(this.grid.isLeftToRight()?"ltr":"rtl")+"\" class=\"dojoxGridSelect\">"];
for(var i=0,o,v;((o=this.options[i])!==undefined)&&((v=this.values[i])!==undefined);i++){
v=v.replace?v.replace(/&/g,"&amp;").replace(/</g,"&lt;"):v;
o=o.replace?o.replace(/&/g,"&amp;").replace(/</g,"&lt;"):o;
if(this.textDir||this.grid.textDir){
o=this._enforceTextDirWithUcc(this.textDir||this.grid.textDir,o);
}
h.push("<option",(_1f==v?" selected":"")," value = \""+v+"\"",">",o,"</option>");
}
h.push("</select>");
return h.join("");
}});
_2.extend(_7.ComboBox,{getWidgetPropsCallOrig:_1.grid.cells.ComboBox.prototype.getWidgetProps,getWidgetProps:function(_21){
var ret=this.getWidgetPropsCallOrig(_21);
if(this.textDir||this.grid.textDir){
ret.textDir=this.textDir||this.grid.textDir;
}
return ret;
}});
_2.extend(_7._Widget,{getWidgetPropsCallOrig:_1.grid.cells._Widget.prototype.getWidgetProps,getWidgetProps:function(_22){
var ret=this.getWidgetPropsCallOrig(_22);
if(this.textDir||this.grid.textDir){
ret.textDir=this.textDir||this.grid.textDir;
}
return ret;
}});
});
