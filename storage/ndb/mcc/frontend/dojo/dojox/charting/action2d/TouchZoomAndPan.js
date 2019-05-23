//>>built
define("dojox/charting/action2d/TouchZoomAndPan",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/sniff","./ChartAction","../Element","dojox/gesture/tap","../plot2d/common"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2(_6,{constructor:function(_a){
},render:function(){
if(!this.isDirty()){
return;
}
this.cleanGroup();
this.group.createRect({width:this.chart.dim.width,height:this.chart.dim.height}).setFill("rgba(0,0,0,0)");
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
return _2("dojox.charting.action2d.TouchZoomAndPan",_5,{defaultParams:{axis:"x",scaleFactor:1.2,maxScale:100,enableScroll:true,enableZoom:true},optionalParams:{},constructor:function(_b,_c,_d){
this._listeners=[{eventName:"ontouchstart",methodName:"onTouchStart"},{eventName:"ontouchmove",methodName:"onTouchMove"},{eventName:"ontouchend",methodName:"onTouchEnd"},{eventName:_7.doubletap,methodName:"onDoubleTap"}];
if(!_d){
_d={};
}
this.axis=_d.axis?_d.axis:"x";
this.scaleFactor=_d.scaleFactor?_d.scaleFactor:1.2;
this.maxScale=_d.maxScale?_d.maxScale:100;
this.enableScroll=_d.enableScroll!=undefined?_d.enableScroll:true;
this.enableZoom=_d.enableScroll!=undefined?_d.enableZoom:true;
this._uName="touchZoomPan"+this.axis;
this.connect();
},connect:function(){
this.inherited(arguments);
if(_4("ios")&&this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.addPlot(this._uName,{type:_9});
}
},disconnect:function(){
if(_4("ios")&&this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.removePlot(this._uName);
}
this.inherited(arguments);
},onTouchStart:function(_e){
var _f=this.chart,_10=_f.getAxis(this.axis);
var _11=_e.touches.length;
this._startPageCoord={x:_e.touches[0].pageX,y:_e.touches[0].pageY};
if((this.enableZoom||this.enableScroll)&&_f._delayedRenderHandle){
_f.render();
}
if(this.enableZoom&&_11>=2){
this._endPageCoord={x:_e.touches[1].pageX,y:_e.touches[1].pageY};
var _12={x:(this._startPageCoord.x+this._endPageCoord.x)/2,y:(this._startPageCoord.y+this._endPageCoord.y)/2};
var _13=_10.getScaler();
this._initScale=_10.getWindowScale();
var t=this._initData=this.plot.toData();
this._middleCoord=t(_12)[this.axis];
this._startCoord=_13.bounds.from;
this._endCoord=_13.bounds.to;
}else{
if(this.enableScroll){
this._startScroll(_10);
_3.stop(_e);
}
}
},onTouchMove:function(_14){
var _15=this.chart,_16=_15.getAxis(this.axis);
var _17=_14.touches.length;
var _18=_16.vertical?"pageY":"pageX",_19=_16.vertical?"y":"x";
if(this.enableZoom&&_17>=2){
var _1a={x:(_14.touches[1].pageX+_14.touches[0].pageX)/2,y:(_14.touches[1].pageY+_14.touches[0].pageY)/2};
var _1b=(this._endPageCoord[_19]-this._startPageCoord[_19])/(_14.touches[1][_18]-_14.touches[0][_18]);
if(this._initScale/_1b>this.maxScale){
return;
}
var _1c=this._initData(_1a)[this.axis];
var _1d=_1b*(this._startCoord-_1c)+this._middleCoord,_1e=_1b*(this._endCoord-_1c)+this._middleCoord;
_15.zoomIn(this.axis,[_1d,_1e]);
_3.stop(_14);
}else{
if(this.enableScroll){
var _1f=_16.vertical?(this._startPageCoord[_19]-_14.touches[0][_18]):(_14.touches[0][_18]-this._startPageCoord[_19]);
_15.setAxisWindow(this.axis,this._lastScale,this._initOffset-_1f/this._lastFactor/this._lastScale);
_15.delayedRender();
_3.stop(_14);
}
}
},onTouchEnd:function(_20){
var _21=this.chart,_22=_21.getAxis(this.axis);
if(_20.touches.length==1&&this.enableScroll){
this._startPageCoord={x:_20.touches[0].pageX,y:_20.touches[0].pageY};
this._startScroll(_22);
}
},_startScroll:function(_23){
var _24=_23.getScaler().bounds;
this._initOffset=_23.getWindowOffset();
this._lastScale=_23.getWindowScale();
this._lastFactor=_24.span/(_24.upper-_24.lower);
},onDoubleTap:function(_25){
var _26=this.chart,_27=_26.getAxis(this.axis);
var _28=1/this.scaleFactor;
if(_27.getWindowScale()==1){
var _29=_27.getScaler(),_2a=_29.bounds.from,end=_29.bounds.to,_2b=(_2a+end)/2,_2c=this.plot.toData(this._startPageCoord)[this.axis],_2d=_28*(_2a-_2b)+_2c,_2e=_28*(end-_2b)+_2c;
_26.zoomIn(this.axis,[_2d,_2e]);
}else{
_26.setAxisWindow(this.axis,1,0);
_26.render();
}
_3.stop(_25);
}});
});
