//>>built
define("dojox/gauges/AnalogLineIndicator",["dojo/_base/declare","./AnalogIndicatorBase"],function(_1,_2){
return _1("dojox.gauges.AnalogLineIndicator",[_2],{_getShapes:function(_3){
var _4=this.direction;
var _5=this.length;
if(_4=="inside"){
_5=-_5;
}
return [_3.createLine({x1:0,y1:-this.offset,x2:0,y2:-_5-this.offset}).setStroke({color:this.color,width:this.width})];
}});
});
