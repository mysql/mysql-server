//>>built
define("dojox/geo/charting/TouchInteractionSupport",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/connect","dojo/_base/window"],function(_1,_2,_3,_4,_5){
return _2("dojox.geo.charting.TouchInteractionSupport",null,{_map:null,_centerTouchLocation:null,_touchMoveListener:null,_touchEndListener:null,_touchEndTapListener:null,_touchStartListener:null,_initialFingerSpacing:null,_initialScale:null,_tapCount:null,_tapThreshold:null,_lastTap:null,_doubleTapPerformed:false,_oneFingerTouch:false,_tapCancel:false,constructor:function(_6){
this._map=_6;
this._centerTouchLocation={x:0,y:0};
this._tapCount=0;
this._lastTap={x:0,y:0};
this._tapThreshold=100;
},connect:function(){
this._touchStartListener=this._map.surface.connect("touchstart",this,this._touchStartHandler);
},disconnect:function(){
if(this._touchStartListener){
_4.disconnect(this._touchStartListener);
this._touchStartListener=null;
}
},_getTouchBarycenter:function(_7){
var _8=_7.touches;
var _9=_8[0];
var _a=null;
if(_8.length>1){
_a=_8[1];
}else{
_a=_8[0];
}
var _b=this._map._getContainerBounds();
var _c=(_9.pageX+_a.pageX)/2-_b.x;
var _d=(_9.pageY+_a.pageY)/2-_b.y;
return {x:_c,y:_d};
},_getFingerSpacing:function(_e){
var _f=_e.touches;
var _10=-1;
if(_f.length>=2){
var dx=(_f[1].pageX-_f[0].pageX);
var dy=(_f[1].pageY-_f[0].pageY);
_10=Math.sqrt(dx*dx+dy*dy);
}
return _10;
},_isDoubleTap:function(_11){
var _12=false;
var _13=_11.touches;
if((this._tapCount>0)&&_13.length==1){
var dx=(_13[0].pageX-this._lastTap.x);
var dy=(_13[0].pageY-this._lastTap.y);
var _14=dx*dx+dy*dy;
if(_14<this._tapThreshold){
_12=true;
}else{
this._tapCount=0;
}
}
this._tapCount++;
this._lastTap.x=_13[0].pageX;
this._lastTap.y=_13[0].pageY;
setTimeout(_1.hitch(this,function(){
this._tapCount=0;
}),300);
return _12;
},_doubleTapHandler:function(_15){
var _16=this._getFeatureFromTouchEvent(_15);
if(_16){
this._map.fitToMapArea(_16._bbox,15,true);
}else{
var _17=_15.touches;
var _18=this._map._getContainerBounds();
var _19=_17[0].pageX-_18.x;
var _1a=_17[0].pageY-_18.y;
var _1b=this._map.screenCoordsToMapCoords(_19,_1a);
this._map.setMapCenterAndScale(_1b.x,_1b.y,this._map.getMapScale()*2,true);
}
},_getFeatureFromTouchEvent:function(_1c){
var _1d=null;
if(_1c.gfxTarget&&_1c.gfxTarget.getParent){
_1d=this._map.mapObj.features[_1c.gfxTarget.getParent().id];
}
return _1d;
},_touchStartHandler:function(_1e){
_3.stop(_1e);
this._oneFingerTouch=(_1e.touches.length==1);
this._tapCancel=!this._oneFingerTouch;
this._doubleTapPerformed=false;
if(this._isDoubleTap(_1e)){
this._doubleTapHandler(_1e);
this._doubleTapPerformed=true;
return;
}
var _1f=this._getTouchBarycenter(_1e);
var _20=this._map.screenCoordsToMapCoords(_1f.x,_1f.y);
this._centerTouchLocation.x=_20.x;
this._centerTouchLocation.y=_20.y;
this._initialFingerSpacing=this._getFingerSpacing(_1e);
this._initialScale=this._map.getMapScale();
if(!this._touchMoveListener){
this._touchMoveListener=_4.connect(_5.global,"touchmove",this,this._touchMoveHandler);
}
if(!this._touchEndTapListener){
this._touchEndTapListener=this._map.surface.connect("touchend",this,this._touchEndTapHandler);
}
if(!this._touchEndListener){
this._touchEndListener=_4.connect(_5.global,"touchend",this,this._touchEndHandler);
}
},_touchEndTapHandler:function(_21){
var _22=_21.touches;
if(_22.length==0){
if(this._oneFingerTouch&&!this._tapCancel){
this._oneFingerTouch=false;
setTimeout(_1.hitch(this,function(){
if(!this._doubleTapPerformed){
var dx=(_21.changedTouches[0].pageX-this._lastTap.x);
var dy=(_21.changedTouches[0].pageY-this._lastTap.y);
var _23=dx*dx+dy*dy;
if(_23<this._tapThreshold){
this._singleTapHandler(_21);
}
}
}),350);
}
this._tapCancel=false;
}
},_touchEndHandler:function(_24){
_3.stop(_24);
var _25=_24.touches;
if(_25.length==0){
if(this._touchMoveListener){
_4.disconnect(this._touchMoveListener);
this._touchMoveListener=null;
}
if(this._touchEndListener){
_4.disconnect(this._touchEndListener);
this._touchEndListener=null;
}
}else{
var _26=this._getTouchBarycenter(_24);
var _27=this._map.screenCoordsToMapCoords(_26.x,_26.y);
this._centerTouchLocation.x=_27.x;
this._centerTouchLocation.y=_27.y;
}
},_singleTapHandler:function(_28){
var _29=this._getFeatureFromTouchEvent(_28);
if(_29){
_29._onclickHandler(_28);
}else{
for(var _2a in this._map.mapObj.features){
this._map.mapObj.features[_2a].select(false);
}
this._map.onFeatureClick(null);
}
},_touchMoveHandler:function(_2b){
_3.stop(_2b);
if(!this._tapCancel){
var dx=(_2b.touches[0].pageX-this._lastTap.x),dy=(_2b.touches[0].pageY-this._lastTap.y);
var _2c=dx*dx+dy*dy;
if(_2c>this._tapThreshold){
this._tapCancel=true;
}
}
var _2d=this._getTouchBarycenter(_2b);
var _2e=this._map.screenCoordsToMapCoords(_2d.x,_2d.y),_2f=_2e.x-this._centerTouchLocation.x,_30=_2e.y-this._centerTouchLocation.y;
var _31=1;
var _32=_2b.touches;
if(_32.length>=2){
var _33=this._getFingerSpacing(_2b);
_31=_33/this._initialFingerSpacing;
this._map.setMapScale(this._initialScale*_31);
}
var _34=this._map.getMapCenter();
this._map.setMapCenter(_34.x-_2f,_34.y-_30);
}});
});
