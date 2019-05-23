//>>built
define("dojox/charting/SimpleTheme",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/Color","dojox/lang/utils","dojox/gfx/gradutils"],function(_1,_2,_3,_4,_5,_6){
var _7=_3("dojox.charting.SimpleTheme",null,{shapeSpaces:{shape:1,shapeX:1,shapeY:1},constructor:function(_8){
_8=_8||{};
var _9=_7.defaultTheme;
_2.forEach(["chart","plotarea","axis","grid","series","marker","indicator"],function(_a){
this[_a]=_1.delegate(_9[_a],_8[_a]);
},this);
if(_8.seriesThemes&&_8.seriesThemes.length){
this.colors=null;
this.seriesThemes=_8.seriesThemes.slice(0);
}else{
this.seriesThemes=null;
this.colors=(_8.colors||_7.defaultColors).slice(0);
}
this.markerThemes=null;
if(_8.markerThemes&&_8.markerThemes.length){
this.markerThemes=_8.markerThemes.slice(0);
}
this.markers=_8.markers?_1.clone(_8.markers):_1.delegate(_7.defaultMarkers);
this.noGradConv=_8.noGradConv;
this.noRadialConv=_8.noRadialConv;
if(_8.reverseFills){
this.reverseFills();
}
this._current=0;
this._buildMarkerArray();
},clone:function(){
var _b=new this.constructor({chart:this.chart,plotarea:this.plotarea,axis:this.axis,grid:this.grid,series:this.series,marker:this.marker,colors:this.colors,markers:this.markers,indicator:this.indicator,seriesThemes:this.seriesThemes,markerThemes:this.markerThemes,noGradConv:this.noGradConv,noRadialConv:this.noRadialConv});
_2.forEach(["clone","clear","next","skip","addMixin","post","getTick"],function(_c){
if(this.hasOwnProperty(_c)){
_b[_c]=this[_c];
}
},this);
return _b;
},clear:function(){
this._current=0;
},next:function(_d,_e,_f){
var _10=_5.merge,_11,_12;
if(this.colors){
_11=_1.delegate(this.series);
_12=_1.delegate(this.marker);
var _13=new _4(this.colors[this._current%this.colors.length]),old;
if(_11.stroke&&_11.stroke.color){
_11.stroke=_1.delegate(_11.stroke);
old=new _4(_11.stroke.color);
_11.stroke.color=new _4(_13);
_11.stroke.color.a=old.a;
}else{
_11.stroke={color:_13};
}
if(_12.stroke&&_12.stroke.color){
_12.stroke=_1.delegate(_12.stroke);
old=new _4(_12.stroke.color);
_12.stroke.color=new _4(_13);
_12.stroke.color.a=old.a;
}else{
_12.stroke={color:_13};
}
if(!_11.fill||_11.fill.type){
_11.fill=_13;
}else{
old=new _4(_11.fill);
_11.fill=new _4(_13);
_11.fill.a=old.a;
}
if(!_12.fill||_12.fill.type){
_12.fill=_13;
}else{
old=new _4(_12.fill);
_12.fill=new _4(_13);
_12.fill.a=old.a;
}
}else{
_11=this.seriesThemes?_10(this.series,this.seriesThemes[this._current%this.seriesThemes.length]):this.series;
_12=this.markerThemes?_10(this.marker,this.markerThemes[this._current%this.markerThemes.length]):_11;
}
var _14=_12&&_12.symbol||this._markers[this._current%this._markers.length];
var _15={series:_11,marker:_12,symbol:_14};
++this._current;
if(_e){
_15=this.addMixin(_15,_d,_e);
}
if(_f){
_15=this.post(_15,_d);
}
return _15;
},skip:function(){
++this._current;
},addMixin:function(_16,_17,_18,_19){
if(_1.isArray(_18)){
_2.forEach(_18,function(m){
_16=this.addMixin(_16,_17,m);
},this);
}else{
var t={};
if("color" in _18){
if(_17=="line"||_17=="area"){
_1.setObject("series.stroke.color",_18.color,t);
_1.setObject("marker.stroke.color",_18.color,t);
}else{
_1.setObject("series.fill",_18.color,t);
}
}
_2.forEach(["stroke","outline","shadow","fill","font","fontColor","labelWiring"],function(_1a){
var _1b="marker"+_1a.charAt(0).toUpperCase()+_1a.substr(1),b=_1b in _18;
if(_1a in _18){
_1.setObject("series."+_1a,_18[_1a],t);
if(!b){
_1.setObject("marker."+_1a,_18[_1a],t);
}
}
if(b){
_1.setObject("marker."+_1a,_18[_1b],t);
}
});
if("marker" in _18){
t.symbol=_18.marker;
t.symbol=_18.marker;
}
_16=_5.merge(_16,t);
}
if(_19){
_16=this.post(_16,_17);
}
return _16;
},post:function(_1c,_1d){
var _1e=_1c.series.fill,t;
if(!this.noGradConv&&this.shapeSpaces[_1e.space]&&_1e.type=="linear"){
if(_1d=="bar"){
t={x1:_1e.y1,y1:_1e.x1,x2:_1e.y2,y2:_1e.x2};
}else{
if(!this.noRadialConv&&_1e.space=="shape"&&(_1d=="slice"||_1d=="circle")){
t={type:"radial",cx:0,cy:0,r:100};
}
}
if(t){
return _5.merge(_1c,{series:{fill:t}});
}
}
return _1c;
},getTick:function(_1f,_20){
var _21=this.axis.tick,_22=_1f+"Tick",_23=_5.merge;
if(_21){
if(this.axis[_22]){
_21=_23(_21,this.axis[_22]);
}
}else{
_21=this.axis[_22];
}
if(_20){
if(_21){
if(_20[_22]){
_21=_23(_21,_20[_22]);
}
}else{
_21=_20[_22];
}
}
return _21;
},inspectObjects:function(f){
_2.forEach(["chart","plotarea","axis","grid","series","marker","indicator"],function(_24){
f(this[_24]);
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
o.fill=_6.reverse(o.fill);
}
});
},addMarker:function(_25,_26){
this.markers[_25]=_26;
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
_1.mixin(_7,{defaultMarkers:{CIRCLE:"m-3,0 c0,-4 6,-4 6,0 m-6,0 c0,4 6,4 6,0",SQUARE:"m-3,-3 l0,6 6,0 0,-6 z",DIAMOND:"m0,-3 l3,3 -3,3 -3,-3 z",CROSS:"m0,-3 l0,6 m-3,-3 l6,0",X:"m-3,-3 l6,6 m0,-6 l-6,6",TRIANGLE:"m-3,3 l3,-6 3,6 z",TRIANGLE_INVERTED:"m-3,-3 l3,6 3,-6 z"},defaultColors:["#54544c","#858e94","#6e767a","#948585","#474747"],defaultTheme:{chart:{stroke:null,fill:"white",pageStyle:null,titleGap:20,titlePos:"top",titleFont:"normal normal bold 14pt Tahoma",titleFontColor:"#333"},plotarea:{stroke:null,fill:"white"},axis:{stroke:{color:"#333",width:1},tick:{color:"#666",position:"center",font:"normal normal normal 7pt Tahoma",fontColor:"#333",labelGap:4},majorTick:{width:1,length:6},minorTick:{width:0.8,length:3},microTick:{width:0.5,length:1},title:{gap:15,font:"normal normal normal 11pt Tahoma",fontColor:"#333",orientation:"axis"}},series:{stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",font:"normal normal normal 8pt Tahoma",fontColor:"#000",labelWiring:{width:1,color:"#ccc"}},marker:{stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",font:"normal normal normal 8pt Tahoma",fontColor:"#000"},indicator:{lineStroke:{width:1.5,color:"#333"},lineOutline:{width:0.1,color:"#ccc"},lineShadow:null,stroke:{width:1.5,color:"#333"},outline:{width:0.1,color:"#ccc"},shadow:null,fill:"#ccc",radius:3,font:"normal normal normal 10pt Tahoma",fontColor:"#000",markerFill:"#ccc",markerSymbol:"m-3,0 c0,-4 6,-4 6,0 m-6,0 c0,4 6,4 6,0",markerStroke:{width:1.5,color:"#333"},markerOutline:{width:0.1,color:"#ccc"},markerShadow:null}}});
return _7;
});
