//>>built
define("dojox/treemap/ScaledLabel",["dojo/_base/declare","dojo/dom-geometry","dojo/dom-construct","dojo/dom-style"],function(_1,_2,_3,_4){
return _1("dojox.treemap.ScaledLabel",null,{onRendererUpdated:function(_5){
if(_5.kind=="leaf"){
var _6=_5.renderer;
var _7=_4.get(_6,"fontSize");
_4.set(_6.firstChild,"fontSize",_7);
_7=parseInt(_7);
var _8=0.75*_2.getContentBox(_6).w/_2.getMarginBox(_6.firstChild).w;
var _9=_2.getContentBox(_6).h/_2.getMarginBox(_6.firstChild).h;
var _a=_2.getContentBox(_6).w-_2.getMarginBox(_6.firstChild).w;
var _b=_2.getContentBox(_6).h-_2.getMarginBox(_6.firstChild).h;
var _c=Math.floor(_7*Math.min(_8,_9));
while(_b>0&&_a>0){
_4.set(_6.firstChild,"fontSize",_c+"px");
_a=_2.getContentBox(_6).w-_2.getMarginBox(_6.firstChild).w;
_b=_2.getContentBox(_6).h-_2.getMarginBox(_6.firstChild).h;
_7=_c;
_c+=1;
}
if(_b<0||_a<0){
_4.set(_6.firstChild,"fontSize",_7+"px");
}
}
},createRenderer:function(_d,_e,_f){
var _10=this.inherited(arguments);
if(_f=="leaf"){
var p=_3.create("div");
_4.set(p,{"position":"absolute","width":"auto"});
_3.place(p,_10);
}
return _10;
},styleRenderer:function(_11,_12,_13,_14){
if(_14!="leaf"){
this.inherited(arguments);
}else{
_4.set(_11,"background",this.getColorForItem(_12).toHex());
_11.firstChild.innerHTML=this.getLabelForItem(_12);
}
}});
});
