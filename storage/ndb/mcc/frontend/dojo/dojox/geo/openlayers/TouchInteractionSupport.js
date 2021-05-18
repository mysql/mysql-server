//>>built
define("dojox/geo/openlayers/TouchInteractionSupport",["dojo/_base/declare","dojo/_base/connect","dojo/_base/html","dojo/_base/lang","dojo/_base/event","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.geo.openlayers.TouchInteractionSupport",null,{_map:null,_centerTouchLocation:null,_touchMoveListener:null,_touchEndListener:null,_initialFingerSpacing:null,_initialScale:null,_tapCount:null,_tapThreshold:null,_lastTap:null,constructor:function(_7){
this._map=_7;
this._centerTouchLocation=new OpenLayers.LonLat(0,0);
var _8=this._map.div;
_2.connect(_8,"touchstart",this,this._touchStartHandler);
_2.connect(_8,"touchmove",this,this._touchMoveHandler);
_2.connect(_8,"touchend",this,this._touchEndHandler);
this._tapCount=0;
this._lastTap={x:0,y:0};
this._tapThreshold=100;
},_getTouchBarycenter:function(_9){
var _a=_9.touches;
var _b=_a[0];
var _c=null;
if(_a.length>1){
_c=_a[1];
}else{
_c=_a[0];
}
var _d=_3.marginBox(this._map.div);
var _e=(_b.pageX+_c.pageX)/2-_d.l;
var _f=(_b.pageY+_c.pageY)/2-_d.t;
return {x:_e,y:_f};
},_getFingerSpacing:function(_10){
var _11=_10.touches;
var _12=-1;
if(_11.length>=2){
var dx=(_11[1].pageX-_11[0].pageX);
var dy=(_11[1].pageY-_11[0].pageY);
_12=Math.sqrt(dx*dx+dy*dy);
}
return _12;
},_isDoubleTap:function(_13){
var _14=false;
var _15=_13.touches;
if((this._tapCount>0)&&_15.length==1){
var dx=(_15[0].pageX-this._lastTap.x);
var dy=(_15[0].pageY-this._lastTap.y);
var _16=dx*dx+dy*dy;
if(_16<this._tapThreshold){
_14=true;
}else{
this._tapCount=0;
}
}
this._tapCount++;
this._lastTap.x=_15[0].pageX;
this._lastTap.y=_15[0].pageY;
setTimeout(_4.hitch(this,function(){
this._tapCount=0;
}),300);
return _14;
},_doubleTapHandler:function(_17){
var _18=_17.touches;
var _19=_3.marginBox(this._map.div);
var _1a=_18[0].pageX-_19.l;
var _1b=_18[0].pageY-_19.t;
var _1c=this._map.getLonLatFromPixel(new OpenLayers.Pixel(_1a,_1b));
this._map.setCenter(new OpenLayers.LonLat(_1c.lon,_1c.lat),this._map.getZoom()+1);
},_touchStartHandler:function(_1d){
_5.stop(_1d);
if(this._isDoubleTap(_1d)){
this._doubleTapHandler(_1d);
return;
}
var _1e=this._getTouchBarycenter(_1d);
this._centerTouchLocation=this._map.getLonLatFromPixel(new OpenLayers.Pixel(_1e.x,_1e.y));
this._initialFingerSpacing=this._getFingerSpacing(_1d);
this._initialScale=this._map.getScale();
if(!this._touchMoveListener){
this._touchMoveListener=_2.connect(_6.global,"touchmove",this,this._touchMoveHandler);
}
if(!this._touchEndListener){
this._touchEndListener=_2.connect(_6.global,"touchend",this,this._touchEndHandler);
}
},_touchEndHandler:function(_1f){
_5.stop(_1f);
var _20=_1f.touches;
if(_20.length==0){
if(this._touchMoveListener){
_2.disconnect(this._touchMoveListener);
this._touchMoveListener=null;
}
if(this._touchEndListener){
_2.disconnect(this._touchEndListener);
this._touchEndListener=null;
}
}else{
var _21=this._getTouchBarycenter(_1f);
this._centerTouchLocation=this._map.getLonLatFromPixel(new OpenLayers.Pixel(_21.x,_21.y));
}
},_touchMoveHandler:function(_22){
_5.stop(_22);
var _23=this._getTouchBarycenter(_22);
var _24=this._map.getLonLatFromPixel(new OpenLayers.Pixel(_23.x,_23.y));
var _25=_24.lon-this._centerTouchLocation.lon;
var _26=_24.lat-this._centerTouchLocation.lat;
var _27=1;
var _28=_22.touches;
if(_28.length>=2){
var _29=this._getFingerSpacing(_22);
_27=_29/this._initialFingerSpacing;
this._map.zoomToScale(this._initialScale/_27);
}
var _2a=this._map.getCenter();
this._map.setCenter(new OpenLayers.LonLat(_2a.lon-_25,_2a.lat-_26));
}});
});
