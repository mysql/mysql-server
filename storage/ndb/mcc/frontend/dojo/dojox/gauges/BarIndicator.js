//>>built
define("dojox/gauges/BarIndicator",["dojo/_base/declare","dojo/_base/fx","dojo/_base/connect","dojo/_base/lang","./BarLineIndicator"],function(_1,fx,_2,_3,_4){
return _1("dojox.gauges.BarIndicator",[_4],{_getShapes:function(_5){
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
var _6=this._gauge._getPosition(v);
if(_6==this.dataX){
_6=this.dataX+1;
}
var y=this._gauge.dataY+Math.floor((this._gauge.dataHeight-this.width)/2)+this.offset;
var _7=[];
_7[0]=_5.createRect({x:this._gauge.dataX,y:y,width:_6-this._gauge.dataX,height:this.width});
_7[0].setStroke({color:this.color});
_7[0].setFill(this.color);
_7[1]=_5.createLine({x1:this._gauge.dataX,y1:y,x2:_6,y2:y});
_7[1].setStroke({color:this.highlight});
if(this.highlight2){
y--;
_7[2]=_5.createLine({x1:this._gauge.dataX,y1:y,x2:_6,y2:y});
_7[2].setStroke({color:this.highlight2});
}
return _7;
},_createShapes:function(_8){
for(var i in this.shape.children){
i=this.shape.children[i];
var _9={};
for(var j in i){
_9[j]=i[j];
}
if(i.shape.type=="line"){
_9.shape.x2=_8+_9.shape.x1;
}else{
if(i.shape.type=="rect"){
_9.width=_8;
}
}
i.setShape(_9);
}
},_move:function(_a){
var c;
var v=this.value;
if(v<this.min){
v=this.min;
}
if(v>this.max){
v=this.max;
}
c=this._gauge._getPosition(this.currentValue);
this.currentValue=v;
v=this._gauge._getPosition(v)-this._gauge.dataX;
if(_a){
this._createShapes(v);
}else{
if(c!=v){
var _b=new fx.Animation({curve:[c,v],duration:this.duration,easing:this.easing});
_2.connect(_b,"onAnimate",_3.hitch(this,this._createShapes));
_b.play();
}
}
}});
});
