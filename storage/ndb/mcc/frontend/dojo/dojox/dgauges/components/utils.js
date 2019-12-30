//>>built
define("dojox/dgauges/components/utils",["dojo/_base/lang","dojo/_base/Color"],function(_1,_2){
var _3={};
_1.mixin(_3,{brightness:function(_4,b){
var _5=_1.mixin(null,_4);
_5.r=Math.max(Math.min(_5.r+b,255),0);
_5.g=Math.max(Math.min(_5.g+b,255),0);
_5.b=Math.max(Math.min(_5.b+b,255),0);
return _5;
},createGradient:function(_6){
var _7={colors:[]};
var _8;
for(var i=0;i<_6.length;i++){
if(i%2==0){
_8={offset:_6[i]};
}else{
_8.color=_6[i];
_7.colors.push(_8);
}
}
return _7;
},_setter:function(_9,_a,_b){
for(var i=0;i<_a.length;i++){
_9[_a[i]]=_b[i];
}
},genericCircularGauge:function(_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
var _17=["originX","originY","radius","startAngle","endAngle","orientation","font","labelPosition","tickShapeFunc"];
if(!_13){
_13="clockwise";
}
if(!_14){
_14={family:"Helvetica",style:"normal",size:"10pt",color:"#555555"};
}
if(!_15){
_15="inside";
}
if(!_16){
_16=function(_18,_19,_1a){
var _1b=_19.tickStroke;
var _1c;
var _1d;
if(_1b){
_1c={color:_1b.color?_1b.color:"#000000",width:_1b.width?_1b.width:0.5};
var col=new _2(_1b.color).toRgb();
_1d={color:_1b.color?_3.brightness({r:col[0],g:col[1],b:col[2]},51):"#000000",width:_1b.width?_1b.width*0.6:0.3};
}
return _18.createLine({x1:_1a.isMinor?2:0,y1:0,x2:_1a.isMinor?8:10,y2:0}).setStroke(_1a.isMinor?_1d:_1c);
};
}
this._setter(_c,_17,[_e,_f,_10,_11,_12,_13,_14,_15,_16]);
_d.set("interactionArea","gauge");
_d.set("indicatorShapeFunc",function(_1e,_1f){
return _1e.createPolyline([0,-5,_1f.scale.radius-6,0,0,5,0,-5]).setStroke({color:"#333333",width:0.25}).setFill(_c._gauge.indicatorColor);
});
}});
return _3;
});
