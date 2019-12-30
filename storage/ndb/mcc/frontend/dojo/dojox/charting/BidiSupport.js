//>>built
define("dojox/charting/BidiSupport",["../main","dojo/_base/lang","dojo/dom-style","dojo/_base/array","dojo/_base/sniff","dojo/dom","dojo/dom-construct","dojox/gfx","dojox/gfx/_gfxBidiSupport","./Chart","./axis2d/common","dojox/string/BidiEngine","dojox/lang/functional"],function(_1,_2,_3,_4,_5,_6,_7,g,_8,_9,da,_a,df){
var _b=new _a();
var dc=_2.getObject("charting",true,_1);
_2.extend(_9,{textDir:"",getTextDir:function(_c){
var _d=this.textDir=="auto"?_b.checkContextual(_c):this.textDir;
if(!_d){
_d=_3.get(this.node,"direction");
}
return _d;
},postscript:function(_e,_f){
var _10=_f?(_f["textDir"]?_11(_f["textDir"]):""):"";
_10=_10?_10:_3.get(this.node,"direction");
this.textDir=_10;
this.surface.textDir=_10;
this.htmlElementsRegistry=[];
this.truncatedLabelsRegistry=[];
},setTextDir:function(_12,obj){
if(_12==this.textDir){
return this;
}
if(_11(_12)!=null){
this.textDir=_12;
this.surface.setTextDir(_12);
if(this.truncatedLabelsRegistry&&_12=="auto"){
_4.forEach(this.truncatedLabelsRegistry,function(_13){
var _14=this.getTextDir(_13["label"]);
if(_13["element"].textDir!=_14){
_13["element"].setShape({textDir:_14});
}
},this);
}
var _15=df.keys(this.axes);
if(_15.length>0){
_4.forEach(_15,function(key,_16,arr){
var _17=this.axes[key];
if(_17.htmlElements[0]){
_17.dirty=true;
_17.render(this.dim,this.offsets);
}
},this);
if(this.title){
var _18=(g.renderer=="canvas"),_19=_18||!_5("ie")&&!_5("opera")?"html":"gfx",_1a=g.normalizedLength(g.splitFontString(this.titleFont).size);
_7.destroy(this.chartTitle);
this.chartTitle=null;
this.chartTitle=da.createText[_19](this,this.surface,this.dim.width/2,this.titlePos=="top"?_1a+this.margins.t:this.dim.height-this.margins.b,"middle",this.title,this.titleFont,this.titleFontColor);
}
}else{
_4.forEach(this.htmlElementsRegistry,function(_1b,_1c,arr){
var _1d=_12=="auto"?this.getTextDir(_1b[4]):_12;
if(_1b[0].children[0]&&_1b[0].children[0].dir!=_1d){
_6.destroy(_1b[0].children[0]);
_1b[0].children[0]=da.createText["html"](this,this.surface,_1b[1],_1b[2],_1b[3],_1b[4],_1b[5],_1b[6]).children[0];
}
},this);
}
}
},truncateBidi:function(_1e,_1f,_20){
if(_20=="gfx"){
this.truncatedLabelsRegistry.push({element:_1e,label:_1f});
if(this.textDir=="auto"){
_1e.setShape({textDir:this.getTextDir(_1f)});
}
}
if(_20=="html"&&this.textDir=="auto"){
_1e.children[0].dir=this.getTextDir(_1f);
}
}});
var _21=function(obj,_22,_23,_24,_25){
if(_23){
var old=obj.prototype[_22];
obj.prototype[_22]=function(){
var _26;
if(_24){
_26=_24.apply(this,arguments);
}
var r=old.apply(this,_26);
if(_25){
r=_25.call(this,r,arguments);
}
return r;
};
}else{
var old=_2.clone(obj[_22]);
obj[_22]=function(){
var _27;
if(_24){
_27=_24.apply(this,arguments);
}
var r=old.apply(this,arguments);
if(_25){
_25(r,arguments);
}
return r;
};
}
};
var _28=function(_29,_2a,_2b,_2c,_2d,_2e){
var _2f=(_3.get(_2a.node,"direction")=="rtl");
var _30=(_2a.getTextDir(_2b)=="rtl");
if(_30&&!_2f){
_2b="<span dir='rtl'>"+_2b+"</span>";
}
if(!_30&&_2f){
_2b="<span dir='ltr'>"+_2b+"</span>";
}
return arguments;
};
if(dc.axis2d&&dc.axis2d.Default){
_21(dc.axis2d.Default,"labelTooltip",true,_28,null);
}
function _31(r,_32){
_32[0].htmlElementsRegistry.push([r,_32[2],_32[3],_32[4],_32[5],_32[6],_32[7]]);
};
_21(da.createText,"html",false,null,_31);
function _11(_33){
return /^(ltr|rtl|auto)$/.test(_33)?_33:null;
};
return _9;
});
