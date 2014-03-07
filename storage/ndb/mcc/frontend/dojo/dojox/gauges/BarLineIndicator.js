//>>built
define("dojox/gauges/BarLineIndicator",["dojo/_base/declare","dojo/_base/fx","dojo/_base/connect","dojo/_base/lang","dojox/gfx","./_Indicator"],function(_1,fx,_2,_3,_4,_5){
return _1("dojox.gauges.BarLineIndicator",[_5],{width:1,_getShapes:function(_6){
if(!this._gauge){
return null;
}
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
var _7=this._gauge._getPosition(v);
var _8=[];
if(this.width>1){
_8[0]=_6.createRect({x:0,y:this._gauge.dataY+this.offset,width:this.width,height:this.length});
_8[0].setStroke({color:this.color});
_8[0].setFill(this.color);
_8[0].setTransform(_4.matrix.translate(_7,0));
}else{
_8[0]=_6.createLine({x1:0,y1:this._gauge.dataY+this.offset,x2:0,y2:this._gauge.dataY+this.offset+this.length});
_8[0].setStroke({color:this.color});
_8[0].setTransform(_4.matrix.translate(_7,0));
}
return _8;
},draw:function(_9,_a){
var i;
if(this.shape){
this._move(_a);
}else{
if(this.shape){
this.shape.parent.remove(this.shape);
this.shape=null;
}
if(this.text){
this.text.parent.remove(this.text);
this.text=null;
}
this.color=this.color||"#000000";
this.length=this.length||this._gauge.dataHeight;
this.width=this.width||3;
this.offset=this.offset||0;
this.highlight=this.highlight||"#4D4D4D";
this.highlight2=this.highlight2||"#A3A3A3";
var _b=this._getShapes(_9,this._gauge,this);
if(_b.length>1){
this.shape=_9.createGroup();
for(var s=0;s<_b.length;s++){
this.shape.add(_b[s]);
}
}else{
this.shape=_b[0];
}
if(this.label){
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
var _c=this._gauge._getPosition(v);
if(this.direction=="inside"){
var _d=this.font?this.font:_4.defaultFont;
var fz=_d.size;
var th=_4.normalizedLength(fz);
this.text=this._gauge.drawText(_9,""+this.label,_c,this._gauge.dataY+this.offset+this.length+5+th,"middle",this.color,this.font);
}else{
this.text=this._gauge.drawText(_9,""+this.label,_c,this._gauge.dataY+this.offset-5,"middle",this.color,this.font);
}
}
this.shape.connect("onmouseover",this,this.handleMouseOver);
this.shape.connect("onmouseout",this,this.handleMouseOut);
this.shape.connect("onmousedown",this,this.handleMouseDown);
this.shape.connect("touchstart",this,this.handleTouchStart);
this.currentValue=this.value;
}
},_move:function(_e){
var v=this.value;
if(v<this._gauge.min){
v=this._gauge.min;
}
if(v>this._gauge.max){
v=this._gauge.max;
}
var c=this._gauge._getPosition(this.currentValue);
this.currentValue=v;
v=this._gauge._getPosition(v);
if(_e||(c==v)){
this.shape.setTransform(_4.matrix.translate(v,0));
}else{
var _f=new fx.Animation({curve:[c,v],duration:this.duration,easing:this.easing});
_2.connect(_f,"onAnimate",_3.hitch(this,function(_10){
if(this.shape){
this.shape.setTransform(_4.matrix.translate(_10,0));
}
}));
_f.play();
}
}});
});
