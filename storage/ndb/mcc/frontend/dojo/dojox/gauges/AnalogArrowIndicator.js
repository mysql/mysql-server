//>>built
define("dojox/gauges/AnalogArrowIndicator",["dojo/_base/declare","./AnalogIndicatorBase"],function(_1,_2){
return _1("dojox.gauges.AnalogArrowIndicator",[_2],{_getShapes:function(_3){
if(!this._gauge){
return null;
}
var _4=this.color?this.color:"black";
var _5=this.strokeColor?this.strokeColor:_4;
var _6={color:_5,width:1};
if(this.color.type&&!this.strokeColor){
_6.color=this.color.colors[0].color;
}
var x=Math.floor(this.width/2);
var _7=this.width*5;
var _8=(this.width&1);
var _9=[];
var _a=[{x:-x,y:0},{x:-x,y:-this.length+_7},{x:-2*x,y:-this.length+_7},{x:0,y:-this.length},{x:2*x+_8,y:-this.length+_7},{x:x+_8,y:-this.length+_7},{x:x+_8,y:0},{x:-x,y:0}];
_9[0]=_3.createPolyline(_a).setStroke(_6).setFill(_4);
_9[1]=_3.createLine({x1:-x,y1:0,x2:-x,y2:-this.length+_7}).setStroke({color:this.highlight});
_9[2]=_3.createLine({x1:-x-3,y1:-this.length+_7,x2:0,y2:-this.length}).setStroke({color:this.highlight});
_9[3]=_3.createCircle({cx:0,cy:0,r:this.width}).setStroke(_6).setFill(_4);
return _9;
}});
});
