//>>built
define("dojox/gauges/AnalogGauge",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojox/gfx","./_Gauge","./AnalogLineIndicator","dojo/dom-geometry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.gauges.AnalogGauge",_8,{startAngle:-90,endAngle:90,cx:0,cy:0,radius:0,orientation:"clockwise",_defaultIndicator:_9,startup:function(){
if(this.getChildren){
_3.forEach(this.getChildren(),function(_b){
_b.startup();
});
}
this.startAngle=Number(this.startAngle);
this.endAngle=Number(this.endAngle);
this.cx=Number(this.cx);
if(!this.cx){
this.cx=this.width/2;
}
this.cy=Number(this.cy);
if(!this.cy){
this.cy=this.height/2;
}
this.radius=Number(this.radius);
if(!this.radius){
this.radius=Math.min(this.cx,this.cy)-25;
}
this.inherited(arguments);
},_getAngle:function(_c){
var v=Number(_c);
var _d;
if(_c==null||isNaN(v)||v<=this.min){
_d=this._mod360(this.startAngle);
}else{
if(v>=this.max){
_d=this._mod360(this.endAngle);
}else{
var _e=this._mod360(this.startAngle);
var _f=(v-this.min);
if(this.orientation!="clockwise"){
_f=-_f;
}
_d=this._mod360(_e+this._getAngleRange()*_f/Math.abs(this.min-this.max));
}
}
return _d;
},_getValueForAngle:function(_10){
var _11=this._mod360(this.startAngle);
var _12=this._mod360(this.endAngle);
if(!this._angleInRange(_10)){
var _13=this._mod360(_11-_10);
var _14=360-_13;
var _15=this._mod360(_12-_10);
var _16=360-_15;
if(Math.min(_13,_14)<Math.min(_15,_16)){
return this.min;
}else{
return this.max;
}
}else{
var _17=Math.abs(this.max-this.min);
var _18=this._mod360(this.orientation=="clockwise"?(_10-_11):(-_10+_11));
return this.min+_17*_18/this._getAngleRange();
}
},_getAngleRange:function(){
var _19;
var _1a=this._mod360(this.startAngle);
var _1b=this._mod360(this.endAngle);
if(_1a==_1b){
return 360;
}
if(this.orientation=="clockwise"){
if(_1b<_1a){
_19=360-(_1a-_1b);
}else{
_19=_1b-_1a;
}
}else{
if(_1b<_1a){
_19=_1a-_1b;
}else{
_19=360-(_1b-_1a);
}
}
return _19;
},_angleInRange:function(_1c){
var _1d=this._mod360(this.startAngle);
var _1e=this._mod360(this.endAngle);
if(_1d==_1e){
return true;
}
_1c=this._mod360(_1c);
if(this.orientation=="clockwise"){
if(_1d<_1e){
return _1c>=_1d&&_1c<=_1e;
}else{
return !(_1c>_1e&&_1c<_1d);
}
}else{
if(_1d<_1e){
return !(_1c>_1d&&_1c<_1e);
}else{
return _1c>=_1e&&_1c<=_1d;
}
}
},_isScaleCircular:function(){
return (this._mod360(this.startAngle)==this._mod360(this.endAngle));
},_mod360:function(v){
while(v>360){
v=v-360;
}
while(v<0){
v=v+360;
}
return v;
},_getRadians:function(_1f){
return _1f*Math.PI/180;
},_getDegrees:function(_20){
return _20*180/Math.PI;
},drawRange:function(_21,_22){
var _23;
if(_22.shape){
_22.shape.parent.remove(_22.shape);
_22.shape=null;
}
var a1,a2;
if((_22.low==this.min)&&(_22.high==this.max)&&((this._mod360(this.endAngle)==this._mod360(this.startAngle)))){
_23=_21.createCircle({cx:this.cx,cy:this.cy,r:this.radius});
}else{
a1=this._getRadians(this._getAngle(_22.low));
a2=this._getRadians(this._getAngle(_22.high));
if(this.orientation=="cclockwise"){
var a=a2;
a2=a1;
a1=a;
}
var x1=this.cx+this.radius*Math.sin(a1),y1=this.cy-this.radius*Math.cos(a1),x2=this.cx+this.radius*Math.sin(a2),y2=this.cy-this.radius*Math.cos(a2),big=0;
var _24;
if(a1<=a2){
_24=a2-a1;
}else{
_24=2*Math.PI-a1+a2;
}
if(_24>Math.PI){
big=1;
}
_23=_21.createPath();
if(_22.size){
_23.moveTo(this.cx+(this.radius-_22.size)*Math.sin(a1),this.cy-(this.radius-_22.size)*Math.cos(a1));
}else{
_23.moveTo(this.cx,this.cy);
}
_23.lineTo(x1,y1);
_23.arcTo(this.radius,this.radius,0,big,1,x2,y2);
if(_22.size){
_23.lineTo(this.cx+(this.radius-_22.size)*Math.sin(a2),this.cy-(this.radius-_22.size)*Math.cos(a2));
_23.arcTo((this.radius-_22.size),(this.radius-_22.size),0,big,0,this.cx+(this.radius-_22.size)*Math.sin(a1),this.cy-(this.radius-_22.size)*Math.cos(a1));
}
_23.closePath();
}
if(_4.isArray(_22.color)||_4.isString(_22.color)){
_23.setStroke({color:_22.color});
_23.setFill(_22.color);
}else{
if(_22.color.type){
a1=this._getRadians(this._getAngle(_22.low));
a2=this._getRadians(this._getAngle(_22.high));
_22.color.x1=this.cx+(this.radius*Math.sin(a1))/2;
_22.color.x2=this.cx+(this.radius*Math.sin(a2))/2;
_22.color.y1=this.cy-(this.radius*Math.cos(a1))/2;
_22.color.y2=this.cy-(this.radius*Math.cos(a2))/2;
_23.setFill(_22.color);
_23.setStroke({color:_22.color.colors[0].color});
}else{
if(_7.svg){
_23.setStroke({color:"green"});
_23.setFill("green");
_23.getEventSource().setAttribute("class",_22.color.style);
}
}
}
_23.connect("onmouseover",_4.hitch(this,this._handleMouseOverRange,_22));
_23.connect("onmouseout",_4.hitch(this,this._handleMouseOutRange,_22));
_22.shape=_23;
},getRangeUnderMouse:function(e){
var _25=null,pos=_a.getContentBox(this.gaugeContent),x=e.clientX-pos.x,y=e.clientY-pos.y,r=Math.sqrt((y-this.cy)*(y-this.cy)+(x-this.cx)*(x-this.cx));
if(r<this.radius){
var _26=this._getDegrees(Math.atan2(y-this.cy,x-this.cx)+Math.PI/2),_27=this._getValueForAngle(_26);
if(this._rangeData){
for(var i=0;(i<this._rangeData.length)&&!_25;i++){
if((Number(this._rangeData[i].low)<=_27)&&(Number(this._rangeData[i].high)>=_27)){
_25=this._rangeData[i];
}
}
}
}
return _25;
},_dragIndicator:function(_28,e){
this._dragIndicatorAt(_28,e.pageX,e.pageY);
_6.stop(e);
},_dragIndicatorAt:function(_29,x,y){
var pos=_a.position(_29.gaugeContent,true),xf=x-pos.x,yf=y-pos.y,_2a=_29._getDegrees(Math.atan2(yf-_29.cy,xf-_29.cx)+Math.PI/2);
var _2b=_29._getValueForAngle(_2a);
_2b=Math.min(Math.max(_2b,_29.min),_29.max);
_29._drag.value=_29._drag.currentValue=_2b;
_29._drag.onDragMove(_29._drag);
_29._drag.draw(this._indicatorsGroup,true);
_29._drag.valueChanged();
}});
});
