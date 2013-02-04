//>>built
define("dojox/charting/action2d/TouchZoomAndPan",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/sniff","./ChartAction","../Element","dojox/gesture/tap","../plot2d/common"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2("dojox.charting.action2d._GlassView",[_6],{constructor:function(_a){
},render:function(){
if(!this.isDirty()){
return;
}
this.cleanGroup();
this.group.createRect({width:this.chart.dim.width,height:this.chart.dim.height}).setFill("rgba(0,0,0,0)");
},cleanGroup:function(_b){
this.inherited(arguments);
return this;
},clear:function(){
this.dirty=true;
if(this.chart.stack[0]!=this){
this.chart.movePlotToFront(this.name);
}
return this;
},getSeriesStats:function(){
return _1.delegate(_8.defaultStats);
},initializeScalers:function(){
return this;
},isDirty:function(){
return this.dirty;
}});
return _2("dojox.charting.action2d.TouchZoomAndPan",_5,{defaultParams:{axis:"x",scaleFactor:1.2,maxScale:100,enableScroll:true,enableZoom:true},optionalParams:{},constructor:function(_c,_d,_e){
this._listeners=[{eventName:"ontouchstart",methodName:"onTouchStart"},{eventName:"ontouchmove",methodName:"onTouchMove"},{eventName:"ontouchend",methodName:"onTouchEnd"},{eventName:_7.doubletap,methodName:"onDoubleTap"}];
if(!_e){
_e={};
}
this.axis=_e.axis?_e.axis:"x";
this.scaleFactor=_e.scaleFactor?_e.scaleFactor:1.2;
this.maxScale=_e.maxScale?_e.maxScale:100;
this.enableScroll=_e.enableScroll!=undefined?_e.enableScroll:true;
this.enableZoom=_e.enableScroll!=undefined?_e.enableZoom:true;
this._uName="touchZoomPan"+this.axis;
this.connect();
},connect:function(){
this.inherited(arguments);
if(_4("safari")&&this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.addPlot(this._uName,{type:_9});
}
},disconnect:function(){
if(_4("safari")&&this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.removePlot(this._uName);
}
this.inherited(arguments);
},onTouchStart:function(_f){
var _10=this.chart,_11=_10.getAxis(this.axis);
var _12=_f.touches.length;
this._startPageCoord={x:_f.touches[0].pageX,y:_f.touches[0].pageY};
if((this.enableZoom||this.enableScroll)&&_10._delayedRenderHandle){
clearTimeout(_10._delayedRenderHandle);
_10._delayedRenderHandle=null;
_10.render();
}
if(this.enableZoom&&_12>=2){
this._endPageCoord={x:_f.touches[1].pageX,y:_f.touches[1].pageY};
var _13={x:(this._startPageCoord.x+this._endPageCoord.x)/2,y:(this._startPageCoord.y+this._endPageCoord.y)/2};
var _14=_11.getScaler();
this._initScale=_11.getWindowScale();
var t=this._initData=this.plot.toData();
this._middleCoord=t(_13)[this.axis];
this._startCoord=_14.bounds.from;
this._endCoord=_14.bounds.to;
}else{
if(this.enableScroll){
this._startScroll(_11);
_3.stop(_f);
}
}
},onTouchMove:function(_15){
var _16=this.chart,_17=_16.getAxis(this.axis);
var _18=_15.touches.length;
var _19=_17.vertical?"pageY":"pageX",_1a=_17.vertical?"y":"x";
if(this.enableZoom&&_18>=2){
var _1b={x:(_15.touches[1].pageX+_15.touches[0].pageX)/2,y:(_15.touches[1].pageY+_15.touches[0].pageY)/2};
var _1c=(this._endPageCoord[_1a]-this._startPageCoord[_1a])/(_15.touches[1][_19]-_15.touches[0][_19]);
if(this._initScale/_1c>this.maxScale){
return;
}
var _1d=this._initData(_1b)[this.axis];
var _1e=_1c*(this._startCoord-_1d)+this._middleCoord,_1f=_1c*(this._endCoord-_1d)+this._middleCoord;
_16.zoomIn(this.axis,[_1e,_1f]);
_3.stop(_15);
}else{
if(this.enableScroll){
var _20=_17.vertical?(this._startPageCoord[_1a]-_15.touches[0][_19]):(_15.touches[0][_19]-this._startPageCoord[_1a]);
_16.setAxisWindow(this.axis,this._lastScale,this._initOffset-_20/this._lastFactor/this._lastScale);
_16.delayedRender();
_3.stop(_15);
}
}
},onTouchEnd:function(_21){
var _22=this.chart,_23=_22.getAxis(this.axis);
if(_21.touches.length==1&&this.enableScroll){
this._startPageCoord={x:_21.touches[0].pageX,y:_21.touches[0].pageY};
this._startScroll(_23);
}
},_startScroll:function(_24){
var _25=_24.getScaler().bounds;
this._initOffset=_24.getWindowOffset();
this._lastScale=_24.getWindowScale();
this._lastFactor=_25.span/(_25.upper-_25.lower);
},onDoubleTap:function(_26){
var _27=this.chart,_28=_27.getAxis(this.axis);
var _29=1/this.scaleFactor;
if(_28.getWindowScale()==1){
var _2a=_28.getScaler(),_2b=_2a.bounds.from,end=_2a.bounds.to,_2c=(_2b+end)/2,_2d=this.plot.toData(this._startPageCoord)[this.axis],_2e=_29*(_2b-_2c)+_2d,_2f=_29*(end-_2c)+_2d;
_27.zoomIn(this.axis,[_2e,_2f]);
}else{
_27.setAxisWindow(this.axis,1,0);
_27.render();
}
_3.stop(_26);
}});
});
