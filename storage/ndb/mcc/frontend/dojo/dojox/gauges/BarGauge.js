//>>built
define("dojox/gauges/BarGauge",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/html","dojo/_base/event","dojox/gfx","./_Gauge","./BarLineIndicator","dojo/dom-geometry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dojox.gauges.BarGauge",_7,{dataX:5,dataY:5,dataWidth:0,dataHeight:0,_defaultIndicator:_8,startup:function(){
if(this.getChildren){
_3.forEach(this.getChildren(),function(_a){
_a.startup();
});
}
if(!this.dataWidth){
this.dataWidth=this.gaugeWidth-10;
}
if(!this.dataHeight){
this.dataHeight=this.gaugeHeight-10;
}
this.inherited(arguments);
},_getPosition:function(_b){
return this.dataX+Math.floor((_b-this.min)/(this.max-this.min)*this.dataWidth);
},_getValueForPosition:function(_c){
return (_c-this.dataX)*(this.max-this.min)/this.dataWidth+this.min;
},drawRange:function(_d,_e){
if(_e.shape){
_e.shape.parent.remove(_e.shape);
_e.shape=null;
}
var x1=this._getPosition(_e.low);
var x2=this._getPosition(_e.high);
var _f=_d.createRect({x:x1,y:this.dataY,width:x2-x1,height:this.dataHeight});
if(_2.isArray(_e.color)||_2.isString(_e.color)){
_f.setStroke({color:_e.color});
_f.setFill(_e.color);
}else{
if(_e.color.type){
var y=this.dataY+this.dataHeight/2;
_e.color.x1=x1;
_e.color.x2=x2;
_e.color.y1=y;
_e.color.y2=y;
_f.setFill(_e.color);
_f.setStroke({color:_e.color.colors[0].color});
}else{
if(_6.svg){
_f.setStroke({color:"green"});
_f.setFill("green");
_f.getEventSource().setAttribute("class",_e.color.style);
}
}
}
_f.connect("onmouseover",_2.hitch(this,this._handleMouseOverRange,_e));
_f.connect("onmouseout",_2.hitch(this,this._handleMouseOutRange,_e));
_e.shape=_f;
},getRangeUnderMouse:function(e){
var _10=null;
var pos=_9.getContentBox(this.gaugeContent);
var x=e.clientX-pos.x;
var _11=this._getValueForPosition(x);
if(this._rangeData){
for(var i=0;(i<this._rangeData.length)&&!_10;i++){
if((Number(this._rangeData[i].low)<=_11)&&(Number(this._rangeData[i].high)>=_11)){
_10=this._rangeData[i];
}
}
}
return _10;
},_dragIndicator:function(_12,e){
this._dragIndicatorAt(_12,e.pageX,e.pageY);
_5.stop(e);
},_dragIndicatorAt:function(_13,x,y){
var pos=_9.position(_13.gaugeContent,true);
var xl=x-pos.x;
var _14=_13._getValueForPosition(xl);
if(_14<_13.min){
_14=_13.min;
}
if(_14>_13.max){
_14=_13.max;
}
_13._drag.value=_14;
_13._drag.onDragMove(_13._drag);
_13._drag.draw(this._indicatorsGroup,true);
_13._drag.valueChanged();
}});
});
