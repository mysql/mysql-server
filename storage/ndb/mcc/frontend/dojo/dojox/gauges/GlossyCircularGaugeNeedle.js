//>>built
define("dojox/gauges/GlossyCircularGaugeNeedle",["dojo/_base/declare","dojo/_base/Color","./AnalogIndicatorBase"],function(_1,_2,_3){
return _1("dojox.gauges.GlossyCircularGaugeNeedle",[_3],{interactionMode:"gauge",color:"#c4c4c4",_getShapes:function(_4){
var _5=_2.blendColors(new _2(this.color),new _2("black"),0.3);
if(!this._gauge){
return null;
}
var _6=[];
_6[0]=_4.createGroup();
var _7=Math.min((this._gauge.width/this._gauge._designWidth),(this._gauge.height/this._gauge._designHeight));
_6[0].createGroup().setTransform({xx:_7,xy:0,yx:0,yy:_7,dx:0,dy:0});
_6[0].children[0].createPath({path:"M357.1429 452.005 L333.0357 465.9233 L333.0357 438.0868 L357.1429 452.005 Z"}).setTransform({xx:0,xy:1,yx:-6.21481,yy:0,dx:-452.00505,dy:2069.75519}).setFill(this.color).setStroke({color:_5,width:1,style:"Solid",cap:"butt",join:20});
return _6;
}});
});
