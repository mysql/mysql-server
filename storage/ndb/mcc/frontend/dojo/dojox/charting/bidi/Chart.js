//>>built
define("dojox/charting/bidi/Chart",["dojox/main","dojo/_base/declare","dojo/_base/lang","dojo/dom-style","dojo/_base/array","dojo/sniff","dojo/dom","dojo/dom-construct","dojox/gfx","dojox/gfx/_gfxBidiSupport","../axis2d/common","dojox/string/BidiEngine","dojox/lang/functional","dojo/dom-attr","./_bidiutils"],function(_1,_2,_3,_4,_5,_6,_7,_8,g,_9,da,_a,df,_b,_c){
var _d=new _a();
var dc=_3.getObject("charting",true,_1);
function _e(_f){
return /^(ltr|rtl|auto)$/.test(_f)?_f:null;
};
return _2(null,{textDir:"",dir:"",isMirrored:false,getTextDir:function(_10){
var _11=this.textDir=="auto"?_d.checkContextual(_10):this.textDir;
if(!_11){
_11=_4.get(this.node,"direction");
}
return _11;
},postscript:function(_12,_13){
var _14=_13?(_13["textDir"]?_e(_13["textDir"]):""):"";
_14=_14?_14:_4.get(this.node,"direction");
this.textDir=_14;
this.surface.textDir=_14;
this.htmlElementsRegistry=[];
this.truncatedLabelsRegistry=[];
var _15="ltr";
if(_b.has(_12,"direction")){
_15=_b.get(_12,"direction");
}
this.setDir(_13?(_13.dir?_13.dir:_15):_15);
},setTextDir:function(_16,obj){
if(_16==this.textDir){
return this;
}
if(_e(_16)!=null){
this.textDir=_16;
this.surface.setTextDir(_16);
if(this.truncatedLabelsRegistry&&_16=="auto"){
_5.forEach(this.truncatedLabelsRegistry,function(_17){
var _18=this.getTextDir(_17["label"]);
if(_17["element"].textDir!=_18){
_17["element"].setShape({textDir:_18});
}
},this);
}
var _19=df.keys(this.axes);
if(_19.length>0){
_5.forEach(_19,function(key,_1a,arr){
var _1b=this.axes[key];
if(_1b.htmlElements[0]){
_1b.dirty=true;
_1b.render(this.dim,this.offsets);
}
},this);
if(this.title){
this._renderTitle(this.dim,this.offsets);
}
}else{
_5.forEach(this.htmlElementsRegistry,function(_1c,_1d,arr){
var _1e=_16=="auto"?this.getTextDir(_1c[4]):_16;
if(_1c[0].children[0]&&_1c[0].children[0].dir!=_1e){
_8.destroy(_1c[0].children[0]);
_1c[0].children[0]=da.createText["html"](this,this.surface,_1c[1],_1c[2],_1c[3],_1c[4],_1c[5],_1c[6]).children[0];
}
},this);
}
}
return this;
},setDir:function(dir){
if(dir=="rtl"||dir=="ltr"){
if(this.dir!=dir){
this.isMirrored=true;
this.dirty=true;
}
this.dir=dir;
}
return this;
},isRightToLeft:function(){
return this.dir=="rtl";
},applyMirroring:function(_1f,dim,_20){
_c.reverseMatrix(_1f,dim,_20,this.dir=="rtl");
_4.set(this.node,"direction","ltr");
return this;
},formatTruncatedLabel:function(_21,_22,_23){
this.truncateBidi(_21,_22,_23);
},truncateBidi:function(_24,_25,_26){
if(_26=="gfx"){
this.truncatedLabelsRegistry.push({element:_24,label:_25});
if(this.textDir=="auto"){
_24.setShape({textDir:this.getTextDir(_25)});
}
}
if(_26=="html"&&this.textDir=="auto"){
_24.children[0].dir=this.getTextDir(_25);
}
},render:function(){
this.inherited(arguments);
this.isMirrored=false;
return this;
},_resetLeftBottom:function(_27){
if(_27.vertical&&this.isMirrored){
_27.opt.leftBottom=!_27.opt.leftBottom;
}
}});
});
