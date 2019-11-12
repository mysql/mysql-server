//>>built
define("dojox/color/SimpleColorModel",["dojo/_base/array","dojo/_base/declare","dojox/color"],function(_1,_2,_3){
return _2("dojox.color.SimpleColorModel",null,{_startColor:null,_endColor:null,constructor:function(_4,_5){
if(_5!=undefined){
this._startColor=_4;
this._endColor=_5;
}else{
var _6=_4.toHsl();
_6.s=100;
_6.l=85;
this._startColor=_3.fromHsl(_6.h,_6.s,_6.l);
this._startColor.a=_4.a;
_6.l=15;
this._endColor=_3.fromHsl(_6.h,_6.s,_6.l);
this._endColor.a=_4.a;
}
},_getInterpoledValue:function(_7,to,_8){
return (_7+(to-_7)*_8);
},getNormalizedValue:function(_9){
},getColor:function(_a){
var _b=this.getNormalizedValue(_a);
var _c=this._startColor.toHsl();
var _d=this._endColor.toHsl();
var h=this._getInterpoledValue(_c.h,_d.h,_b);
var s=this._getInterpoledValue(_c.s,_d.s,_b);
var l=this._getInterpoledValue(_c.l,_d.l,_b);
var a=this._getInterpoledValue(this._startColor.a,this._endColor.a,_b);
var c=_3.fromHsl(h,s,l);
c.a=a;
return c;
}});
});
