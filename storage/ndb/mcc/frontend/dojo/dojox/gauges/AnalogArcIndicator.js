//>>built
define("dojox/gauges/AnalogArcIndicator",["dojo/_base/declare","dojo/_base/lang","dojo/_base/connect","dojo/_base/fx","./AnalogIndicatorBase"],function(_1,_2,_3,fx,_4){
return _1("dojox.gauges.AnalogArcIndicator",[_4],{_createArc:function(_5){
if(this.shape){
var _6=this._gauge._mod360(this._gauge.startAngle);
var a=this._gauge._getRadians(this._gauge._getAngle(_5));
var sa=this._gauge._getRadians(_6);
if(this._gauge.orientation=="cclockwise"){
var _7=a;
a=sa;
sa=_7;
}
var _8;
var _9=0;
if(sa<=a){
_8=a-sa;
}else{
_8=2*Math.PI+a-sa;
}
if(_8>Math.PI){
_9=1;
}
var _a=Math.cos(a);
var _b=Math.sin(a);
var _c=Math.cos(sa);
var _d=Math.sin(sa);
var _e=this.offset+this.width;
var p=["M"];
p.push(this._gauge.cx+this.offset*_d);
p.push(this._gauge.cy-this.offset*_c);
p.push("A",this.offset,this.offset,0,_9,1);
p.push(this._gauge.cx+this.offset*_b);
p.push(this._gauge.cy-this.offset*_a);
p.push("L");
p.push(this._gauge.cx+_e*_b);
p.push(this._gauge.cy-_e*_a);
p.push("A",_e,_e,0,_9,0);
p.push(this._gauge.cx+_e*_d);
p.push(this._gauge.cy-_e*_c);
p.push("z");
this.shape.setShape(p.join(" "));
this.currentValue=_5;
}
},draw:function(_f,_10){
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
if(this.shape){
if(_10){
this._createArc(v);
}else{
var _11=new fx.Animation({curve:[this.currentValue,v],duration:this.duration,easing:this.easing});
_3.connect(_11,"onAnimate",_2.hitch(this,this._createArc));
_11.play();
}
}else{
var _12=this.color?this.color:"black";
var _13=this.strokeColor?this.strokeColor:_12;
var _14={color:_13,width:1};
if(this.color.type&&!this.strokeColor){
_14.color=this.color.colors[0].color;
}
this.shape=_f.createPath().setStroke(_14).setFill(_12);
this._createArc(v);
this.shape.connect("onmouseover",this,this.handleMouseOver);
this.shape.connect("onmouseout",this,this.handleMouseOut);
this.shape.connect("onmousedown",this,this.handleMouseDown);
this.shape.connect("touchstart",this,this.handleTouchStart);
}
}});
});
