//>>built
define("dojox/charting/Theme",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/Color","dojox/color/_base","dojox/color/Palette","dojox/lang/utils","dojox/gfx/gradutils"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_3("dojox.charting.Theme",null,{shapeSpaces:{shape:1,shapeX:1,shapeY:1},constructor:function(_a){
_a=_a||{};
var _b=_9.defaultTheme;
_2.forEach(["chart","plotarea","axis","series","marker","indicator"],function(_c){
this[_c]=_1.delegate(_b[_c],_a[_c]);
},this);
if(_a.seriesThemes&&_a.seriesThemes.length){
this.colors=null;
this.seriesThemes=_a.seriesThemes.slice(0);
}else{
this.seriesThemes=null;
this.colors=(_a.colors||_9.defaultColors).slice(0);
}
this.markerThemes=null;
if(_a.markerThemes&&_a.markerThemes.length){
this.markerThemes=_a.markerThemes.slice(0);
}
this.markers=_a.markers?_1.clone(_a.markers):_1.delegate(_9.defaultMarkers);
this.noGradConv=_a.noGradConv;
this.noRadialConv=_a.noRadialConv;
if(_a.reverseFills){
this.reverseFills();
}
this._current=0;
this._buildMarkerArray();
},clone:function(){
var _d=new _9({chart:this.chart,plotarea:this.plotarea,axis:this.axis,series:this.series,marker:this.marker,colors:this.colors,markers:this.markers,indicator:this.indicator,seriesThemes:this.seriesThemes,markerThemes:this.markerThemes,noGradConv:this.noGradConv,noRadialConv:this.noRadialConv});
_2.forEach(["clone","clear","next","skip","addMixin","post","getTick"],function(_e){
if(this.hasOwnProperty(_e)){
_d[_e]=this[_e];
}
},this);
return _d;
},clear:function(){
this._current=0;
},next:function(_f,_10,_11){
var _12=_7.merge,_13,_14;
if(this.colors){
_13=_1.delegate(this.series);
_14=_1.delegate(this.marker);
var _15=new _4(this.colors[this._current%this.colors.length]),old;
if(_13.stroke&&_13.stroke.color){
_13.stroke=_1.delegate(_13.stroke);
old=new _4(_13.stroke.color);
_13.stroke.color=new _4(_15);
_13.stroke.color.a=old.a;
}else{
_13.stroke={color:_15};
}
if(_14.stroke&&_14.stroke.color){
_14.stroke=_1.delegate(_14.stroke);
old=new _4(_14.stroke.color);
_14.stroke.color=new _4(_15);
_14.stroke.color.a=old.a;
}else{
_14.stroke={color:_15};
}
if(!_13.fill||_13.fill.type){
_13.fill=_15;
}else{
old=new _4(_13.fill);
_13.fill=new _4(_15);
_13.fill.a=old.a;
}
if(!_14.fill||_14.fill.type){
_14.fill=_15;
}else{
old=new _4(_14.fill);
_14.fill=new _4(_15);
_14.fill.a=old.a;
}
}else{
_13=this.seriesThemes?_12(this.series,this.seriesThemes[this._current%this.seriesThemes.length]):this.series;
_14=this.markerThemes?_12(this.marker,this.markerThemes[this._current%this.markerThemes.length]):_13;
}
var _16=_14&&_14.symbol||this._markers[this._current%this._markers.length];
var _17={series:_13,marker:_14,symbol:_16};
++this._current;
if(_10){
_17=this.addMixin(_17,_f,_10);
}
if(_11){
_17=this.post(_17,_f);
}
return _17;
},skip:function(){
++this._current;
},addMixin:function(_18,_19,_1a,_1b){
if(_1.isArray(_1a)){
_2.forEach(_1a,function(m){
_18=this.addMixin(_18,_19,m);
},this);
}else{
var t={};
if("color" in _1a){
if(_19=="line"||_19=="area"){
_1.setObject("series.stroke.color",_1a.color,t);
_1.setObject("marker.stroke.color",_1a.color,t);
}else{
_1.setObject("series.fill",_1a.color,t);
}
}
_2.forEach(["stroke","outline","shadow","fill","font","fontColor","labelWiring"],function(_1c){
var _1d="marker"+_1c.charAt(0).toUpperCase()+_1c.substr(1),b=_1d in _1a;
if(_1c in _1a){
_1.setObject("series."+_1c,_1a[_1c],t);
if(!b){
_1.setObject("marker."+_1c,_1a[_1c],t);
}
}
if(b){
_1.setObject("marker."+_1c,_1a[_1d],t);
}
});
if("marker" in _1a){
t.symbol=_1a.marker;
}
_18=_7.merge(_18,t);
}
if(_1b){
_18=this.post(_18,_19);
}
return _18;
},post:function(_1e,_1f){
var _20=_1e.series.fill,t;
if(!this.noGradConv&&this.shapeSpaces[_20.space]&&_20.type=="linear"){
if(_1f=="bar"){
t={x1:_20.y1,y1:_20.x1,x2:_20.y2,y2:_20.x2};
}else{
if(!this.noRadialConv&&_20.space=="shape"&&(_1f=="slice"||_1f=="circle")){
t={type:"radial",cx:0,cy:0,r:100};
}
}
if(t){
return _7.merge(_1e,{series:{fill:t}});
}
}
return _1e;
},getTick:function(_21,_22){
var _23=this.axis.tick,_24=_21+"Tick",_25=_7.merge;
if(_23){
if(this.axis[_24]){
_23=_25(_23,this.axis[_24]);
}
}else{
_23=this.axis[_24];
}
if(_22){
if(_23){
if(_22[_24]){
_23=_25(_23,_22[_24]);
}
}else{
_23=_22[_24];
}
}
return _23;
},inspectObjects:function(f){
_2.forEach(["chart","plotarea","axis","series","marker","indicator"],function(_26){
f(this[_26]);
},this);
if(this.seriesThemes){
_2.forEach(this.seriesThemes,f);
}
if(this.markerThemes){
_2.forEach(this.markerThemes,f);
}
},reverseFills:function(){
this.inspectObjects(function(o){
if(o&&o.fill){
o.fill=_8.reverse(o.fill);
}
});
},addMarker:function(_27,_28){
this.markers[_27]=_28;
this._buildMarkerArray();
},setMarkers:function(obj){
this.markers=obj;
this._buildMarkerArray();
},_buildMarkerArray:function(){
this._markers=[];
for(var p in this.markers){
this._markers.push(this.markers[p]);
}
}});
_1.mixin(_9,{defaultMarkers:{CIRCLE:"m-3,0 c0,-4 6,-4 6,0 m-6,0 c0,4 6,4 6,0",SQUARE:"m-3,-3 l0,6 6,0 0,-6 z",DIAMOND:"m0,-3 l3,3 -3,3 -3,-3 z",CROSS:"m0,-3 l0,6 m-3,-3 l6,0",X:"m-3,-3 l6,6 m0,-6 l-6,6",TRIANGLE:"m-3,3 l3,-6 3,6 z",TRIANGLE_INVERTED:"m-3,-3 l3,6 3,-6 z"},defaultColors:["#54544c","#858e94","#6e767a","#948585","#474747"],defaultTheme:{chart:{stroke:null,fill:"white",pageStyle:null,titleGap:20,titlePos:"top",titleFont:"normal normal bold 14pt Tahoma",titleFontColor:"#333"},plotarea:{stroke:null,fill:"white"},axis:{stroke:{color:"#333",width:1},tick:{color:"#666",position:"center",font:"normal normal normal 7pt Tahoma",fontColor:"#333",titleGap:15,titleFont:"normal normal normal 11pt Tahoma",titleFontColor:"#333",titleOrientation:"axis"},majorTick:{width:1,length:6},minorTick:{width:0.8,length:3},microTick:{width:0.5,length:1}},series:{stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",font:"normal normal normal 8pt Tahoma",fontColor:"#000",labelWiring:{width:1,color:"#ccc"}},marker:{stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",font:"normal normal normal 8pt Tahoma",fontColor:"#000"},indicator:{lineStroke:{width:1.5,color:"#333"},lineOutline:{width:0.1,color:"#ccc"},lineShadow:null,stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",radius:3,font:"normal normal normal 10pt Tahoma",fontColor:"#000",markerFill:"#ccc",markerSymbol:"m-3,0 c0,-4 6,-4 6,0 m-6,0 c0,4 6,4 6,0",markerStroke:{width:1.5,color:"#333"},markerOutline:{width:0.1,color:"#ccc"},markerShadow:null}},defineColors:function(_29){
_29=_29||{};
var l,c=[],n=_29.num||5;
if(_29.colors){
l=_29.colors.length;
for(var i=0;i<n;i++){
c.push(_29.colors[i%l]);
}
return c;
}
if(_29.hue){
var s=_29.saturation||100,st=_29.low||30,end=_29.high||90;
l=(end+st)/2;
return _5.Palette.generate(_5.fromHsv(_29.hue,s,l),"monochromatic").colors;
}
if(_29.generator){
return _5.Palette.generate(_29.base,_29.generator).colors;
}
return c;
},generateGradient:function(_2a,_2b,_2c){
var _2d=_1.delegate(_2a);
_2d.colors=[{offset:0,color:_2b},{offset:1,color:_2c}];
return _2d;
},generateHslColor:function(_2e,_2f){
_2e=new _4(_2e);
var hsl=_2e.toHsl(),_30=_5.fromHsl(hsl.h,hsl.s,_2f);
_30.a=_2e.a;
return _30;
},generateHslGradient:function(_31,_32,_33,_34){
_31=new _4(_31);
var hsl=_31.toHsl(),_35=_5.fromHsl(hsl.h,hsl.s,_33),_36=_5.fromHsl(hsl.h,hsl.s,_34);
_35.a=_36.a=_31.a;
return _9.generateGradient(_32,_35,_36);
}});
return _9;
});
