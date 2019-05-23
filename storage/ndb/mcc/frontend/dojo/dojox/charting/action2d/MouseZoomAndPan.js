//>>built
define("dojox/charting/action2d/MouseZoomAndPan",["dojo/_base/declare","dojo/_base/window","dojo/_base/array","dojo/_base/event","dojo/_base/connect","dojo/mouse","./ChartAction","dojo/_base/sniff","dojo/dom-prop","dojo/keys"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_8("mozilla")?3:120;
var _c={none:function(_d){
return !_d.ctrlKey&&!_d.altKey&&!_d.shiftKey;
},ctrl:function(_e){
return _e.ctrlKey&&!_e.altKey&&!_e.shiftKey;
},alt:function(_f){
return !_f.ctrlKey&&_f.altKey&&!_f.shiftKey;
},shift:function(_10){
return !_10.ctrlKey&&!_10.altKey&&_10.shiftKey;
}};
return _1("dojox.charting.action2d.MouseZoomAndPan",_7,{defaultParams:{axis:"x",scaleFactor:1.2,maxScale:100,enableScroll:true,enableDoubleClickZoom:true,enableKeyZoom:true,keyZoomModifier:"ctrl"},optionalParams:{},constructor:function(_11,_12,_13){
this._listeners=[{eventName:_6.wheel,methodName:"onMouseWheel"}];
if(!_13){
_13={};
}
this.axis=_13.axis?_13.axis:"x";
this.scaleFactor=_13.scaleFactor?_13.scaleFactor:1.2;
this.maxScale=_13.maxScale?_13.maxScale:100;
this.enableScroll=_13.enableScroll!=undefined?_13.enableScroll:true;
this.enableDoubleClickZoom=_13.enableDoubleClickZoom!=undefined?_13.enableDoubleClickZoom:true;
this.enableKeyZoom=_13.enableKeyZoom!=undefined?_13.enableKeyZoom:true;
this.keyZoomModifier=_13.keyZoomModifier?_13.keyZoomModifier:"ctrl";
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
},onMouseDown:function(_14){
var _15=this.chart,_16=_15.getAxis(this.axis);
if(!_16.vertical){
this._startCoord=_14.pageX;
}else{
this._startCoord=_14.pageY;
}
this._startOffset=_16.getWindowOffset();
this._isPanning=true;
if(_8("ie")){
this._handles.push(_5.connect(this.chart.node,"onmousemove",this,"onMouseMove"));
this._handles.push(_5.connect(this.chart.node,"onmouseup",this,"onMouseUp"));
this.chart.node.setCapture();
}else{
this._handles.push(_5.connect(_2.doc,"onmousemove",this,"onMouseMove"));
this._handles.push(_5.connect(_2.doc,"onmouseup",this,"onMouseUp"));
}
_15.node.focus();
_4.stop(_14);
},onMouseMove:function(_17){
if(this._isPanning){
var _18=this.chart,_19=_18.getAxis(this.axis);
var _1a=_19.vertical?(this._startCoord-_17.pageY):(_17.pageX-this._startCoord);
var _1b=_19.getScaler().bounds,s=_1b.span/(_1b.upper-_1b.lower);
var _1c=_19.getWindowScale();
_18.setAxisWindow(this.axis,_1c,this._startOffset-_1a/s/_1c);
_18.render();
}
},onMouseUp:function(_1d){
this._isPanning=false;
this._disconnectHandles();
},onMouseWheel:function(_1e){
var _1f=_1e.wheelDelta/_b;
if(_1f>-1&&_1f<0){
_1f=-1;
}else{
if(_1f>0&&_1f<1){
_1f=1;
}
}
this._onZoom(_1f,_1e);
},onKeyPress:function(_20){
if(_c[this.keyZoomModifier](_20)){
if(_20.keyChar=="+"||_20.keyCode==_a.NUMPAD_PLUS){
this._onZoom(1,_20);
}else{
if(_20.keyChar=="-"||_20.keyCode==_a.NUMPAD_MINUS){
this._onZoom(-1,_20);
}
}
}
},onDoubleClick:function(_21){
var _22=this.chart,_23=_22.getAxis(this.axis);
var _24=1/this.scaleFactor;
if(_23.getWindowScale()==1){
var _25=_23.getScaler(),_26=_25.bounds.from,end=_25.bounds.to,_27=(_26+end)/2,_28=this.plot.toData({x:_21.pageX,y:_21.pageY})[this.axis],_29=_24*(_26-_27)+_28,_2a=_24*(end-_27)+_28;
_22.zoomIn(this.axis,[_29,_2a]);
}else{
_22.setAxisWindow(this.axis,1,0);
_22.render();
}
_4.stop(_21);
},_onZoom:function(_2b,_2c){
var _2d=(_2b<0?Math.abs(_2b)*this.scaleFactor:1/(Math.abs(_2b)*this.scaleFactor));
var _2e=this.chart,_2f=_2e.getAxis(this.axis);
var _30=_2f.getWindowScale();
if(_30/_2d>this.maxScale){
return;
}
var _31=_2f.getScaler(),_32=_31.bounds.from,end=_31.bounds.to;
var _33=(_2c.type=="keypress")?(_32+end)/2:this.plot.toData({x:_2c.pageX,y:_2c.pageY})[this.axis];
var _34=_2d*(_32-_33)+_33,_35=_2d*(end-_33)+_33;
_2e.zoomIn(this.axis,[_34,_35]);
_4.stop(_2c);
}});
});
