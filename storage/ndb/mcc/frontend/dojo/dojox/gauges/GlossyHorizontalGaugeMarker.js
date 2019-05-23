//>>built
define("dojox/gauges/GlossyHorizontalGaugeMarker",["dojo/_base/declare","dojo/_base/Color","./BarLineIndicator"],function(_1,_2,_3){
return _1("dojox.gauges.GlossyHorizontalGaugeMarker",[_3],{interactionMode:"gauge",color:"black",_getShapes:function(_4){
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
var _5=this._gauge._getPosition(v);
var _6=[];
var _7=new _2(this.color);
_7.a=0.67;
var _8=_2.blendColors(_7,new _2("white"),0.4);
var _9=_6[0]=_4.createGroup();
var _a=this._gauge.height/100;
_a=Math.max(_a,0.5);
_a=Math.min(_a,1);
_9.setTransform({xx:1,xy:0,yx:0,yy:1,dx:_5,dy:0});
var _b=_9.createGroup().setTransform({xx:1,xy:0,yx:0,yy:1,dx:-_a*10,dy:this._gauge.dataY+this.offset});
var _c=_b.createGroup().setTransform({xx:_a,xy:0,yx:0,yy:_a,dx:0,dy:0});
_c.createRect({x:0.5,y:0,width:20,height:47,r:6}).setFill(_7).setStroke(_8);
_c.createPath({path:"M 10.106 41 L 10.106 6 C 10.106 2.687 7.419 0 4.106 0 L 0.372 0 C -0.738 6.567 1.022 15.113 1.022 23.917 C 1.022 32.721 2.022 40.667 0.372 47 L 4.106 47 C 7.419 47 10.106 44.314 10.106 41 Z"}).setFill(_8).setTransform({xx:1,xy:0,yx:0,yy:1,dx:10.306,dy:0.009});
_c.createRect({x:9.5,y:1.5,width:2,height:34,r:0.833717}).setFill(_7).setStroke(this.color);
_c.createRect({x:9,y:0,width:3,height:34,r:6}).setFill({type:"linear",x1:9,y1:0,x2:9,y2:34,colors:[{offset:0,color:"white"},{offset:1,color:this.color}]});
return _6;
}});
});
