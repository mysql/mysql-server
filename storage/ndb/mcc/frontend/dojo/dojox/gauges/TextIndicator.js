//>>built
define("dojox/gauges/TextIndicator",["dojo/_base/declare","./_Indicator"],function(_1,_2){
return _1("dojox.gauges.TextIndicator",[_2],{x:0,y:0,align:"middle",fixedPrecision:true,precision:0,draw:function(_3,_4){
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
var _5;
var _6=this._gauge?this._gauge._getNumberModule():null;
if(_6){
_5=this.fixedPrecision?_6.format(v,{places:this.precision}):_6.format(v);
}else{
_5=this.fixedPrecision?v.toFixed(this.precision):v.toString();
}
var x=this.x?this.x:0;
var y=this.y?this.y:0;
var _7=this.align?this.align:"middle";
if(!this.shape){
this.shape=_3.createText({x:x,y:y,text:_5,align:_7});
}else{
this.shape.setShape({x:x,y:y,text:_5,align:_7});
}
this.shape.setFill(this.color);
if(this.font){
this.shape.setFont(this.font);
}
}});
});
