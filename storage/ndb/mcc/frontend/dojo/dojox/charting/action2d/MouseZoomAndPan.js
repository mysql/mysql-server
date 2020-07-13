//>>built
define("dojox/charting/action2d/MouseZoomAndPan",["dojo/_base/declare","dojo/_base/window","dojo/_base/array","dojo/_base/event","dojo/_base/connect","dojo/mouse","./ChartAction","dojo/sniff","dojo/dom-prop","dojo/keys","dojo/has!dojo-bidi?../bidi/action2d/ZoomAndPan"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_8("mozilla")?3:120;
var _d={none:function(_e){
return !_e.ctrlKey&&!_e.altKey&&!_e.shiftKey;
},ctrl:function(_f){
return _f.ctrlKey&&!_f.altKey&&!_f.shiftKey;
},alt:function(_10){
return !_10.ctrlKey&&_10.altKey&&!_10.shiftKey;
},shift:function(_11){
return !_11.ctrlKey&&!_11.altKey&&_11.shiftKey;
}};
var _12=_1(_8("dojo-bidi")?"dojox.charting.action2d.NonBidiMouseZoomAndPan":"dojox.charting.action2d.MouseZoomAndPan",_7,{defaultParams:{axis:"x",scaleFactor:1.2,maxScale:100,enableScroll:true,enableDoubleClickZoom:true,enableKeyZoom:true,keyZoomModifier:"ctrl"},optionalParams:{},constructor:function(_13,_14,_15){
this._listeners=[{eventName:_6.wheel,methodName:"onMouseWheel"}];
if(!_15){
_15={};
}
this.axis=_15.axis?_15.axis:"x";
this.scaleFactor=_15.scaleFactor?_15.scaleFactor:1.2;
this.maxScale=_15.maxScale?_15.maxScale:100;
this.enableScroll=_15.enableScroll!=undefined?_15.enableScroll:true;
this.enableDoubleClickZoom=_15.enableDoubleClickZoom!=undefined?_15.enableDoubleClickZoom:true;
this.enableKeyZoom=_15.enableKeyZoom!=undefined?_15.enableKeyZoom:true;
this.keyZoomModifier=_15.keyZoomModifier?_15.keyZoomModifier:"ctrl";
if(this.enableScroll){
this._listeners.push({eventName:"onmousedown",methodName:"onMouseDown"});
}
if(this.enableDoubleClickZoom){
this._listeners.push({eventName:"ondblclick",methodName:"onDoubleClick"});
}
if(this.enableKeyZoom){
this._listeners.push({eventName:"keypress",methodName:"onKeyPress"});
}
this._handles=[];
this.connect();
},_disconnectHandles:function(){
if(_8("ie")){
this.chart.node.releaseCapture();
}
_3.forEach(this._handles,_5.disconnect);
this._handles=[];
},connect:function(){
this.inherited(arguments);
if(this.enableKeyZoom){
_9.set(this.chart.node,"tabindex","0");
}
},disconnect:function(){
this.inherited(arguments);
if(this.enableKeyZoom){
_9.set(this.chart.node,"tabindex","-1");
}
this._disconnectHandles();
},onMouseDown:function(_16){
var _17=this.chart,_18=_17.getAxis(this.axis);
if(!_18.vertical){
this._startCoord=_16.pageX;
}else{
this._startCoord=_16.pageY;
}
this._startOffset=_18.getWindowOffset();
this._isPanning=true;
if(_8("ie")){
this._handles.push(_5.connect(this.chart.node,"onmousemove",this,"onMouseMove"));
this._handles.push(_5.connect(this.chart.node,"onmouseup",this,"onMouseUp"));
this.chart.node.setCapture();
}else{
this._handles.push(_5.connect(_2.doc,"onmousemove",this,"onMouseMove"));
this._handles.push(_5.connect(_2.doc,"onmouseup",this,"onMouseUp"));
}
_17.node.focus();
_4.stop(_16);
},onMouseMove:function(_19){
if(this._isPanning){
var _1a=this.chart,_1b=_1a.getAxis(this.axis);
var _1c=this._getDelta(_19);
var _1d=_1b.getScaler().bounds,s=_1d.span/(_1d.upper-_1d.lower);
var _1e=_1b.getWindowScale();
_1a.setAxisWindow(this.axis,_1e,this._startOffset-_1c/s/_1e);
_1a.render();
}
},onMouseUp:function(_1f){
this._isPanning=false;
this._disconnectHandles();
},onMouseWheel:function(_20){
var _21=_20.wheelDelta/_c;
if(_21>-1&&_21<0){
_21=-1;
}else{
if(_21>0&&_21<1){
_21=1;
}
}
this._onZoom(_21,_20);
},onKeyPress:function(_22){
if(_d[this.keyZoomModifier](_22)){
if(_22.keyChar=="+"||_22.keyCode==_a.NUMPAD_PLUS){
this._onZoom(1,_22);
}else{
if(_22.keyChar=="-"||_22.keyCode==_a.NUMPAD_MINUS){
this._onZoom(-1,_22);
}
}
}
},onDoubleClick:function(_23){
var _24=this.chart,_25=_24.getAxis(this.axis);
var _26=1/this.scaleFactor;
if(_25.getWindowScale()==1){
var _27=_25.getScaler(),_28=_27.bounds.from,end=_27.bounds.to,_29=(_28+end)/2,_2a=this.plot.toData({x:_23.pageX,y:_23.pageY})[this.axis],_2b=_26*(_28-_29)+_2a,_2c=_26*(end-_29)+_2a;
_24.zoomIn(this.axis,[_2b,_2c]);
}else{
_24.setAxisWindow(this.axis,1,0);
_24.render();
}
_4.stop(_23);
},_onZoom:function(_2d,_2e){
var _2f=(_2d<0?Math.abs(_2d)*this.scaleFactor:1/(Math.abs(_2d)*this.scaleFactor));
var _30=this.chart,_31=_30.getAxis(this.axis);
var _32=_31.getWindowScale();
if(_32/_2f>this.maxScale){
return;
}
var _33=_31.getScaler(),_34=_33.bounds.from,end=_33.bounds.to;
var _35=(_2e.type=="keypress")?(_34+end)/2:this.plot.toData({x:_2e.pageX,y:_2e.pageY})[this.axis];
var _36=_2f*(_34-_35)+_35,_37=_2f*(end-_35)+_35;
_30.zoomIn(this.axis,[_36,_37]);
_4.stop(_2e);
},_getDelta:function(_38){
return this.chart.getAxis(this.axis).vertical?(this._startCoord-_38.pageY):(_38.pageX-this._startCoord);
}});
return _8("dojo-bidi")?_1("dojox.charting.action2d.MouseZoomAndPan",[_12,_b]):_12;
});
