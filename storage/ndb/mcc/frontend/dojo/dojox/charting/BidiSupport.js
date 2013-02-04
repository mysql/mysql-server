//>>built
define("dojox/charting/BidiSupport",["dojo/_base/lang","dojo/_base/html","dojo/_base/array","dojo/_base/sniff","dojo/dom","dojo/dom-construct","dojox/gfx","dojox/gfx/_gfxBidiSupport","./Chart","./axis2d/common","dojox/string/BidiEngine","dojox/lang/functional"],function(_1,_2,_3,_4,_5,_6,g,_7,_8,da,_9,df){
var _a=new _9();
_1.extend(_8,{textDir:"",getTextDir:function(_b){
var _c=this.textDir=="auto"?_a.checkContextual(_b):this.textDir;
if(!_c){
_c=_2.style(this.node,"direction");
}
return _c;
},postscript:function(_d,_e){
var _f=_e?(_e["textDir"]?_10(_e["textDir"]):""):"";
_f=_f?_f:_2.style(this.node,"direction");
this.textDir=_f;
this.surface.textDir=_f;
this.htmlElementsRegistry=[];
this.truncatedLabelsRegistry=[];
},setTextDir:function(_11,obj){
if(_11==this.textDir){
return this;
}
if(_10(_11)!=null){
this.textDir=_11;
this.surface.setTextDir(_11);
if(this.truncatedLabelsRegistry&&_11=="auto"){
_3.forEach(this.truncatedLabelsRegistry,function(_12){
var _13=this.getTextDir(_12["label"]);
if(_12["element"].textDir!=_13){
_12["element"].setShape({textDir:_13});
}
},this);
}
var _14=df.keys(this.axes);
if(_14.length>0){
_3.forEach(_14,function(key,_15,arr){
var _16=this.axes[key];
if(_16.htmlElements[0]){
_16.dirty=true;
_16.render(this.dim,this.offsets);
}
},this);
if(this.title){
var _17=(g.renderer=="canvas"),_18=_17||!_4("ie")&&!_4("opera")?"html":"gfx",_19=g.normalizedLength(g.splitFontString(this.titleFont).size);
_6.destroy(this.chartTitle);
this.chartTitle=null;
this.chartTitle=da.createText[_18](this,this.surface,this.dim.width/2,this.titlePos=="top"?_19+this.margins.t:this.dim.height-this.margins.b,"middle",this.title,this.titleFont,this.titleFontColor);
}
}else{
_3.forEach(this.htmlElementsRegistry,function(_1a,_1b,arr){
var _1c=_11=="auto"?this.getTextDir(_1a[4]):_11;
if(_1a[0].children[0]&&_1a[0].children[0].dir!=_1c){
_5.destroy(_1a[0].children[0]);
_1a[0].children[0]=da.createText["html"](this,this.surface,_1a[1],_1a[2],_1a[3],_1a[4],_1a[5],_1a[6]).children[0];
}
},this);
}
}
},truncateBidi:function(_1d,_1e,_1f){
if(_1f=="gfx"){
this.truncatedLabelsRegistry.push({element:_1d,label:_1e});
if(this.textDir=="auto"){
_1d.setShape({textDir:this.getTextDir(_1e)});
}
}
if(_1f=="html"&&this.textDir=="auto"){
_1d.children[0].dir=this.getTextDir(_1e);
}
}});
var _20=function(obj,_21,_22,_23,_24){
if(_22){
var old=obj.prototype[_21];
obj.prototype[_21]=function(){
var _25;
if(_23){
_25=_23.apply(this,arguments);
}
var r=old.apply(this,_25);
if(_24){
r=_24.call(this,r,arguments);
}
return r;
};
}else{
var old=_1.clone(obj[_21]);
obj[_21]=function(){
var _26;
if(_23){
_26=_23.apply(this,arguments);
}
var r=old.apply(this,arguments);
if(_24){
_24(r,arguments);
}
return r;
};
}
};
var _27=function(_28,_29,_2a,_2b,_2c,_2d){
var _2e=(_2.style(_29.node,"direction")=="rtl");
var _2f=(_29.getTextDir(_2a)=="rtl");
if(_2f&&!_2e){
_2a="<span dir='rtl'>"+_2a+"</span>";
}
if(!_2f&&_2e){
_2a="<span dir='ltr'>"+_2a+"</span>";
}
return arguments;
};
if(dojox.charting.axis2d&&dojox.charting.axis2d.Default){
_20(dojox.charting.axis2d.Default,"labelTooltip",true,_27,null);
}
function _30(r,_31){
_31[0].htmlElementsRegistry.push([r,_31[2],_31[3],_31[4],_31[5],_31[6],_31[7]]);
};
_20(da.createText,"html",false,null,_30);
function _10(_32){
return /^(ltr|rtl|auto)$/.test(_32)?_32:null;
};
});
