//>>built
define("dojox/gauges/AnalogCircleIndicator",["dojo/_base/declare","./AnalogIndicatorBase"],function(_1,_2){
return _1("dojox.gauges.AnalogCircleIndicator",[_2],{_getShapes:function(_3){
var _4=this.color?this.color:"black";
var _5=this.strokeColor?this.strokeColor:_4;
var _6={color:_5,width:1};
if(this.color.type&&!this.strokeColor){
_6.color=this.color.colors[0].color;
}
return [_3.createCircle({cx:0,cy:-this.offset,r:this.length}).setFill(_4).setStroke(_6)];
}});
});
