//>>built
define("dojox/gauges/BarCircleIndicator",["dojo/_base/declare","dojox/gfx","./BarLineIndicator"],function(_1,_2,_3){
return _1("dojox.gauges.BarCircleIndicator",[_3],{_getShapes:function(_4){
var _5=this.color?this.color:"black";
var _6=this.strokeColor?this.strokeColor:_5;
var _7={color:_6,width:1};
if(this.color.type&&!this.strokeColor){
_7.color=this.color.colors[0].color;
}
var y=this._gauge.dataY+this.offset+this.length/2;
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
var _8=this._gauge._getPosition(v);
var _9=[_4.createCircle({cx:0,cy:y,r:this.length/2}).setFill(_5).setStroke(_7)];
_9[0].setTransform(_2.matrix.translate(_8,0));
return _9;
}});
});
