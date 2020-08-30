//>>built
define("dojox/geo/charting/MouseInteractionSupport",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/connect","dojo/_base/window","dojo/_base/html","dojo/dom","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.geo.charting.MouseInteractionSupport",null,{_map:null,_mapClickLocation:null,_screenClickLocation:null,_mouseDragListener:null,_mouseUpListener:null,_mouseUpClickListener:null,_mouseDownListener:null,_mouseMoveListener:null,_mouseWheelListener:null,_currentFeature:null,_cancelMouseClick:null,_zoomEnabled:false,_panEnabled:false,_onDragStartListener:null,_onSelectStartListener:null,mouseClickThreshold:2,constructor:function(_9,_a){
this._map=_9;
this._mapClickLocation={x:0,y:0};
this._screenClickLocation={x:0,y:0};
this._cancelMouseClick=false;
if(_a){
this._zoomEnabled=_a.enableZoom;
this._panEnabled=_a.enablePan;
if(_a.mouseClickThreshold&&_a.mouseClickThreshold>0){
this.mouseClickThreshold=_a.mouseClickThreshold;
}
}
},setEnableZoom:function(_b){
if(_b&&!this._mouseWheelListener){
var _c=!_8("mozilla")?"onmousewheel":"DOMMouseScroll";
this._mouseWheelListener=this._map.surface.connect(_c,this,this._mouseWheelHandler);
}else{
if(!_b&&this._mouseWheelListener){
_4.disconnect(this._mouseWheelListener);
this._mouseWheelListener=null;
}
}
this._zoomEnabled=_b;
},setEnablePan:function(_d){
this._panEnabled=_d;
},connect:function(){
this._mouseMoveListener=this._map.surface.connect("onmousemove",this,this._mouseMoveHandler);
this._mouseDownListener=this._map.surface.connect("onmousedown",this,this._mouseDownHandler);
if(_8("ie")){
_onDragStartListener=_4.connect(_5.doc,"ondragstart",this,_3.stop);
_onSelectStartListener=_4.connect(_5.doc,"onselectstart",this,_3.stop);
}
this.setEnableZoom(this._zoomEnabled);
this.setEnablePan(this._panEnabled);
},disconnect:function(){
var _e=this._zoomEnabled;
this.setEnableZoom(false);
this._zoomEnabled=_e;
if(this._mouseMoveListener){
_4.disconnect(this._mouseMoveListener);
this._mouseMoveListener=null;
_4.disconnect(this._mouseDownListener);
this._mouseDownListener=null;
}
if(this._onDragStartListener){
_4.disconnect(this._onDragStartListener);
this._onDragStartListener=null;
_4.disconnect(this._onSelectStartListener);
this._onSelectStartListener=null;
}
},_mouseClickHandler:function(_f){
var _10=this._getFeatureFromMouseEvent(_f);
if(_10){
_10._onclickHandler(_f);
}else{
for(var _11 in this._map.mapObj.features){
this._map.mapObj.features[_11].select(false);
}
this._map.onFeatureClick(null);
}
},_mouseDownHandler:function(_12){
_3.stop(_12);
this._map.focused=true;
this._cancelMouseClick=false;
this._screenClickLocation.x=_12.pageX;
this._screenClickLocation.y=_12.pageY;
var _13=this._map._getContainerBounds();
var _14=_12.pageX-_13.x,_15=_12.pageY-_13.y;
var _16=this._map.screenCoordsToMapCoords(_14,_15);
this._mapClickLocation.x=_16.x;
this._mapClickLocation.y=_16.y;
if(!_8("ie")){
this._mouseDragListener=_4.connect(_5.doc,"onmousemove",this,this._mouseDragHandler);
this._mouseUpClickListener=this._map.surface.connect("onmouseup",this,this._mouseUpClickHandler);
this._mouseUpListener=_4.connect(_5.doc,"onmouseup",this,this._mouseUpHandler);
}else{
var _17=_7.byId(this._map.container);
this._mouseDragListener=_4.connect(_17,"onmousemove",this,this._mouseDragHandler);
this._mouseUpClickListener=this._map.surface.connect("onmouseup",this,this._mouseUpClickHandler);
this._mouseUpListener=this._map.surface.connect("onmouseup",this,this._mouseUpHandler);
this._map.surface.rawNode.setCapture();
}
},_mouseUpClickHandler:function(_18){
if(!this._cancelMouseClick){
this._mouseClickHandler(_18);
}
this._cancelMouseClick=false;
},_mouseUpHandler:function(_19){
_3.stop(_19);
this._map.mapObj.marker._needTooltipRefresh=true;
if(this._mouseDragListener){
_4.disconnect(this._mouseDragListener);
this._mouseDragListener=null;
}
if(this._mouseUpClickListener){
_4.disconnect(this._mouseUpClickListener);
this._mouseUpClickListener=null;
}
if(this._mouseUpListener){
_4.disconnect(this._mouseUpListener);
this._mouseUpListener=null;
}
if(_8("ie")){
this._map.surface.rawNode.releaseCapture();
}
},_getFeatureFromMouseEvent:function(_1a){
var _1b=null;
if(_1a.gfxTarget&&_1a.gfxTarget.getParent){
_1b=this._map.mapObj.features[_1a.gfxTarget.getParent().id];
}
return _1b;
},_mouseMoveHandler:function(_1c){
if(this._mouseDragListener&&this._panEnabled){
return;
}
var _1d=this._getFeatureFromMouseEvent(_1c);
if(_1d!=this._currentFeature){
if(this._currentFeature){
this._currentFeature._onmouseoutHandler();
}
this._currentFeature=_1d;
if(_1d){
_1d._onmouseoverHandler();
}
}
if(_1d){
_1d._onmousemoveHandler(_1c);
}
},_mouseDragHandler:function(_1e){
_3.stop(_1e);
var dx=Math.abs(_1e.pageX-this._screenClickLocation.x);
var dy=Math.abs(_1e.pageY-this._screenClickLocation.y);
if(!this._cancelMouseClick&&(dx>this.mouseClickThreshold||dy>this.mouseClickThreshold)){
this._cancelMouseClick=true;
if(this._panEnabled){
this._map.mapObj.marker.hide();
}
}
if(!this._panEnabled){
return;
}
var _1f=this._map._getContainerBounds();
var _20=_1e.pageX-_1f.x,_21=_1e.pageY-_1f.y;
var _22=this._map.screenCoordsToMapCoords(_20,_21);
var _23=_22.x-this._mapClickLocation.x;
var _24=_22.y-this._mapClickLocation.y;
var _25=this._map.getMapCenter();
this._map.setMapCenter(_25.x-_23,_25.y-_24);
},_mouseWheelHandler:function(_26){
_3.stop(_26);
this._map.mapObj.marker.hide();
var _27=this._map._getContainerBounds();
var _28=_26.pageX-_27.x,_29=_26.pageY-_27.y;
var _2a=this._map.screenCoordsToMapCoords(_28,_29);
var _2b=_26[(_8("mozilla")?"detail":"wheelDelta")]/(_8("mozilla")?-3:120);
var _2c=Math.pow(1.2,_2b);
this._map.setMapScaleAt(this._map.getMapScale()*_2c,_2a.x,_2a.y,false);
this._map.mapObj.marker._needTooltipRefresh=true;
}});
});
