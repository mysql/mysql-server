//>>built
define("dojox/gauges/AnalogIndicatorBase",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/fx","dojox/gfx","./_Indicator"],function(_1,_2,_3,fx,_4,_5){
return _2("dojox.gauges.AnalogIndicatorBase",[_5],{draw:function(_6,_7){
if(this.shape){
this._move(_7);
}else{
if(this.text){
this.text.parent.remove(this.text);
this.text=null;
}
var a=this._gauge._getAngle(Math.min(Math.max(this.value,this._gauge.min),this._gauge.max));
this.color=this.color||"#000000";
this.length=this.length||this._gauge.radius;
this.width=this.width||1;
this.offset=this.offset||0;
this.highlight=this.highlight||"#D0D0D0";
var _8=this._getShapes(_6,this._gauge,this);
if(_8){
if(_8.length>1){
this.shape=_6.createGroup();
for(var s=0;s<_8.length;s++){
this.shape.add(_8[s]);
}
}else{
this.shape=_8[0];
}
this.shape.setTransform([{dx:this._gauge.cx,dy:this._gauge.cy},_4.matrix.rotateg(a)]);
this.shape.connect("onmouseover",this,this.handleMouseOver);
this.shape.connect("onmouseout",this,this.handleMouseOut);
this.shape.connect("onmousedown",this,this.handleMouseDown);
this.shape.connect("touchstart",this,this.handleTouchStart);
}
if(this.label){
var _9=this.direction;
if(!_9){
_9="outside";
}
var _a;
if(_9=="inside"){
_a=-this.length+this.offset-5;
}else{
_a=this.length+this.offset+5;
}
var _b=this._gauge._getRadians(90-a);
this._layoutLabel(_6,this.label+"",this._gauge.cx,this._gauge.cy,_a,_b,_9);
}
this.currentValue=this.value;
}
},_layoutLabel:function(_c,_d,ox,oy,_e,_f,_10){
var _11=this.font?this.font:_4.defaultFont;
var box=_4._base._getTextBox(_d,{font:_4.makeFontString(_4.makeParameters(_4.defaultFont,_11))});
var tw=box.w;
var fz=_11.size;
var th=_4.normalizedLength(fz);
var tfx=ox+Math.cos(_f)*_e-tw/2;
var tfy=oy-Math.sin(_f)*_e-th/2;
var _12;
var _13=[];
_12=tfx;
var ipx=_12;
var ipy=-Math.tan(_f)*_12+oy+Math.tan(_f)*ox;
if(ipy>=tfy&&ipy<=tfy+th){
_13.push({x:ipx,y:ipy});
}
_12=tfx+tw;
ipx=_12;
ipy=-Math.tan(_f)*_12+oy+Math.tan(_f)*ox;
if(ipy>=tfy&&ipy<=tfy+th){
_13.push({x:ipx,y:ipy});
}
_12=tfy;
ipx=-1/Math.tan(_f)*_12+ox+1/Math.tan(_f)*oy;
ipy=_12;
if(ipx>=tfx&&ipx<=tfx+tw){
_13.push({x:ipx,y:ipy});
}
_12=tfy+th;
ipx=-1/Math.tan(_f)*_12+ox+1/Math.tan(_f)*oy;
ipy=_12;
if(ipx>=tfx&&ipx<=tfx+tw){
_13.push({x:ipx,y:ipy});
}
var dif;
if(_10=="inside"){
for(var it=0;it<_13.length;it++){
var ip=_13[it];
dif=this._distance(ip.x,ip.y,ox,oy)-_e;
if(dif>=0){
tfx=ox+Math.cos(_f)*(_e-dif)-tw/2;
tfy=oy-Math.sin(_f)*(_e-dif)-th/2;
break;
}
}
}else{
for(it=0;it<_13.length;it++){
ip=_13[it];
dif=this._distance(ip.x,ip.y,ox,oy)-_e;
if(dif<=0){
tfx=ox+Math.cos(_f)*(_e-dif)-tw/2;
tfy=oy-Math.sin(_f)*(_e-dif)-th/2;
break;
}
}
}
this.text=this._gauge.drawText(_c,_d,tfx+tw/2,tfy+th,"middle",this.color,this.font);
},_distance:function(x1,y1,x2,y2){
return Math.sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
},_move:function(_14){
var v=Math.min(Math.max(this.value,this._gauge.min),this._gauge.max),c=this.currentValue;
if(_14){
var _15=this._gauge._getAngle(v);
this.shape.setTransform([{dx:this._gauge.cx,dy:this._gauge.cy},_4.matrix.rotateg(_15)]);
this.currentValue=v;
}else{
if(c!=v){
var _16=new fx.Animation({curve:[c,v],duration:this.duration,easing:this.easing});
_3.connect(_16,"onAnimate",_1.hitch(this,function(_17){
this.shape.setTransform([{dx:this._gauge.cx,dy:this._gauge.cy},_4.matrix.rotateg(this._gauge._getAngle(_17))]);
this.currentValue=_17;
}));
_16.play();
}
}
}});
});
