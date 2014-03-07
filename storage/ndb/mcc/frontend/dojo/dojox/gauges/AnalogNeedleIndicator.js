//>>built
define("dojox/gauges/AnalogNeedleIndicator",["dojo/_base/declare","./AnalogIndicatorBase"],function(_1,_2){
return _1("dojox.gauges.AnalogNeedleIndicator",[_2],{_getShapes:function(_3){
if(!this._gauge){
return null;
}
var x=Math.floor(this.width/2);
var _4=[];
var _5=this.color?this.color:"black";
var _6=this.strokeColor?this.strokeColor:_5;
var _7=this.strokeWidth?this.strokeWidth:1;
var _8={color:_6,width:_7};
if(_5.type&&!this.strokeColor){
_8.color=_5.colors[0].color;
}
var xy=(Math.sqrt(2)*(x));
_4[0]=_3.createPath().setStroke(_8).setFill(_5).moveTo(xy,-xy).arcTo((2*x),(2*x),0,0,0,-xy,-xy).lineTo(0,-this.length).closePath();
_4[1]=_3.createCircle({cx:0,cy:0,r:this.width}).setStroke(_8).setFill(_5);
return _4;
}});
});
