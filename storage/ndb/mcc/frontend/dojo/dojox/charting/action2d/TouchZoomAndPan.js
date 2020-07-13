//>>built
define("dojox/charting/action2d/TouchZoomAndPan",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/sniff","./ChartAction","../Element","dojo/touch","../plot2d/common","dojo/has!dojo-bidi?../bidi/action2d/ZoomAndPan"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_2(_6,{constructor:function(_b){
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
var _c=_2(_4("dojo-bidi")?"dojox.charting.action2d.NonBidiTouchZoomAndPan":"dojox.charting.action2d.TouchZoomAndPan",_5,{defaultParams:{axis:"x",scaleFactor:1.2,maxScale:100,enableScroll:true,enableZoom:true},optionalParams:{},constructor:function(_d,_e,_f){
this._listeners=[{eventName:_7.press,methodName:"onTouchStart"},{eventName:_7.move,methodName:"onTouchMove"},{eventName:_7.release,methodName:"onTouchEnd"}];
if(!_f){
_f={};
}
this.axis=_f.axis?_f.axis:"x";
this.scaleFactor=_f.scaleFactor?_f.scaleFactor:1.2;
this.maxScale=_f.maxScale?_f.maxScale:100;
this.enableScroll=_f.enableScroll!=undefined?_f.enableScroll:true;
this.enableZoom=_f.enableScroll!=undefined?_f.enableZoom:true;
this._uName="touchZoomPan"+this.axis;
this.connect();
},connect:function(){
this.inherited(arguments);
if(this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.addPlot(this._uName,{type:_a});
}
},disconnect:function(){
if(this.chart.surface.declaredClass.indexOf("svg")!=-1){
this.chart.removePlot(this._uName);
}
this.inherited(arguments);
},onTouchStart:function(_10){
var _11=this.chart,_12=_11.getAxis(this.axis);
var _13=_10.touches?_10.touches.length:1;
var _14=_10.touches?_10.touches[0]:_10;
var _15=this._startPageCoord;
this._startPageCoord={x:_14.pageX,y:_14.pageY};
if((this.enableZoom||this.enableScroll)&&_11._delayedRenderHandle){
_11.render();
}
if(this.enableZoom&&_13>=2){
this._startTime=0;
this._endPageCoord={x:_10.touches[1].pageX,y:_10.touches[1].pageY};
var _16={x:(this._startPageCoord.x+this._endPageCoord.x)/2,y:(this._startPageCoord.y+this._endPageCoord.y)/2};
var _17=_12.getScaler();
this._initScale=_12.getWindowScale();
var t=this._initData=this.plot.toData();
this._middleCoord=t(_16)[this.axis];
this._startCoord=_17.bounds.from;
this._endCoord=_17.bounds.to;
}else{
if(!_10.touches||_10.touches.length==1){
if(!this._startTime){
this._startTime=new Date().getTime();
}else{
if((new Date().getTime()-this._startTime)<250&&Math.abs(this._startPageCoord.x-_15.x)<30&&Math.abs(this._startPageCoord.y-_15.y)<30){
this._startTime=0;
this.onDoubleTap(_10);
}else{
this._startTime=0;
}
}
}else{
this._startTime=0;
}
if(this.enableScroll){
this._startScroll(_12);
_3.stop(_10);
}
}
},onTouchMove:function(_18){
var _19=this.chart,_1a=_19.getAxis(this.axis);
var _1b=_18.touches?_18.touches.length:1;
var _1c=_1a.vertical?"pageY":"pageX",_1d=_1a.vertical?"y":"x";
this._startTime=0;
if(this.enableZoom&&_1b>=2){
var _1e={x:(_18.touches[1].pageX+_18.touches[0].pageX)/2,y:(_18.touches[1].pageY+_18.touches[0].pageY)/2};
var _1f=(this._endPageCoord[_1d]-this._startPageCoord[_1d])/(_18.touches[1][_1c]-_18.touches[0][_1c]);
if(this._initScale/_1f>this.maxScale){
return;
}
var _20=this._initData(_1e)[this.axis];
var _21=_1f*(this._startCoord-_20)+this._middleCoord,_22=_1f*(this._endCoord-_20)+this._middleCoord;
_19.zoomIn(this.axis,[_21,_22]);
_3.stop(_18);
}else{
if(this.enableScroll){
var _23=this._getDelta(_18);
_19.setAxisWindow(this.axis,this._lastScale,this._initOffset-_23/this._lastFactor/this._lastScale);
_19.delayedRender();
_3.stop(_18);
}
}
},onTouchEnd:function(_24){
var _25=this.chart,_26=_25.getAxis(this.axis);
if((!_24.touches||_24.touches.length==1)&&this.enableScroll){
var _27=_24.touches?_24.touches[0]:_24;
this._startPageCoord={x:_27.pageX,y:_27.pageY};
this._startScroll(_26);
}
},_startScroll:function(_28){
var _29=_28.getScaler().bounds;
this._initOffset=_28.getWindowOffset();
this._lastScale=_28.getWindowScale();
this._lastFactor=_29.span/(_29.upper-_29.lower);
},onDoubleTap:function(_2a){
var _2b=this.chart,_2c=_2b.getAxis(this.axis);
var _2d=1/this.scaleFactor;
if(_2c.getWindowScale()==1){
var _2e=_2c.getScaler(),_2f=_2e.bounds.from,end=_2e.bounds.to,_30=(_2f+end)/2,_31=this.plot.toData(this._startPageCoord)[this.axis],_32=_2d*(_2f-_30)+_31,_33=_2d*(end-_30)+_31;
_2b.zoomIn(this.axis,[_32,_33]);
}else{
_2b.setAxisWindow(this.axis,1,0);
_2b.render();
}
_3.stop(_2a);
},_getDelta:function(_34){
var _35=this.chart.getAxis(this.axis),_36=_35.vertical?"pageY":"pageX",_37=_35.vertical?"y":"x";
var _38=_34.touches?_34.touches[0]:_34;
return _35.vertical?(this._startPageCoord[_37]-_38[_36]):(_38[_36]-this._startPageCoord[_37]);
}});
return _4("dojo-bidi")?_2("dojox.charting.action2d.TouchZoomAndPan",[_c,_9]):_c;
});
