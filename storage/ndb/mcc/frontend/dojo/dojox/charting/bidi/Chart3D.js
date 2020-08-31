//>>built
define("dojox/charting/bidi/Chart3D",["dojo/_base/declare","dojo/dom-style","dojo/dom-attr","./_bidiutils"],function(_1,_2,_3,_4){
return _1(null,{direction:"",isMirrored:false,postscript:function(_5,_6,_7,_8,_9){
var _a="ltr";
if(_3.has(_5,"direction")){
_a=_3.get(_5,"direction");
}
this.chartBaseDirection=_9?_9:_a;
},generate:function(){
this.inherited(arguments);
this.isMirrored=false;
return this;
},applyMirroring:function(_b,_c,_d){
if(this.isMirrored){
_4.reverseMatrix(_b,_c,_d,this.dir=="rtl");
}
_2.set(this.node,"direction","ltr");
return this;
},setDir:function(_e){
if(_e=="rtl"||_e=="ltr"){
if(this.dir!=_e){
this.isMirrored=true;
}
this.dir=_e;
}
return this;
},isRightToLeft:function(){
return this.dir=="rtl";
}});
});
