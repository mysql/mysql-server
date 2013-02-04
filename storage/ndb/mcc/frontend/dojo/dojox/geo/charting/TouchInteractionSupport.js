//>>built
define("dojox/geo/charting/TouchInteractionSupport",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/connect","dojo/_base/window"],function(_1,_2,_3,_4,_5){
return _2("dojox.geo.charting.TouchInteractionSupport",null,{_map:null,_centerTouchLocation:null,_touchMoveListener:null,_touchEndListener:null,_touchEndTapListener:null,_touchStartListener:null,_initialFingerSpacing:null,_initialScale:null,_tapCount:null,_tapThreshold:null,_lastTap:null,_doubleTapPerformed:false,_oneFingerTouch:false,_tapCancel:false,constructor:function(_6,_7){
this._map=_6;
this._centerTouchLocation={x:0,y:0};
this._tapCount=0;
this._lastTap={x:0,y:0};
this._tapThreshold=100;
},connect:function(){
_touchStartListener=this._map.surface.connect("touchstart",this,this._touchStartHandler);
},disconnect:function(){
if(this._touchStartListener){
_4.disconnect(this._touchStartListener);
this._touchStartListener=null;
}
},_getTouchBarycenter:function(_8){
var _9=_8.touches;
var _a=_9[0];
var _b=null;
if(_9.length>1){
_b=_9[1];
}else{
_b=_9[0];
}
var _c=this._map._getContainerBounds();
var _d=(_a.pageX+_b.pageX)/2-_c.x;
var _e=(_a.pageY+_b.pageY)/2-_c.y;
return {x:_d,y:_e};
},_getFingerSpacing:function(_f){
var _10=_f.touches;
var _11=-1;
if(_10.length>=2){
var dx=(_10[1].pageX-_10[0].pageX);
var dy=(_10[1].pageY-_10[0].pageY);
_11=Math.sqrt(dx*dx+dy*dy);
}
return _11;
},_isDoubleTap:function(_12){
var _13=false;
var _14=_12.touches;
if((this._tapCount>0)&&_14.length==1){
var dx=(_14[0].pageX-this._lastTap.x);
var dy=(_14[0].pageY-this._lastTap.y);
var _15=dx*dx+dy*dy;
if(_15<this._tapThreshold){
_13=true;
}else{
this._tapCount=0;
}
}
this._tapCount++;
this._lastTap.x=_14[0].pageX;
this._lastTap.y=_14[0].pageY;
setTimeout(_1.hitch(this,function(){
this._tapCount=0;
}),300);
return _13;
},_doubleTapHandler:function(_16){
var _17=this._getFeatureFromTouchEvent(_16);
if(_17){
this._map.fitToMapArea(_17._bbox,15,true);
}else{
var _18=_16.touches;
var _19=this._map._getContainerBounds();
var _1a=_18[0].pageX-_19.x;
var _1b=_18[0].pageY-_19.y;
var _1c=this._map.screenCoordsToMapCoords(_1a,_1b);
this._map.setMapCenterAndScale(_1c.x,_1c.y,this._map.getMapScale()*2,true);
}
},_getFeatureFromTouchEvent:function(_1d){
var _1e=null;
if(_1d.gfxTarget&&_1d.gfxTarget.getParent){
_1e=this._map.mapObj.features[_1d.gfxTarget.getParent().id];
}
return _1e;
},_touchStartHandler:function(_1f){
_3.stop(_1f);
this._oneFingerTouch=(_1f.touches.length==1);
this._tapCancel=!this._oneFingerTouch;
this._doubleTapPerformed=false;
if(this._isDoubleTap(_1f)){
this._doubleTapHandler(_1f);
this._doubleTapPerformed=true;
return;
}
var _20=this._getTouchBarycenter(_1f);
var _21=this._map.screenCoordsToMapCoords(_20.x,_20.y);
this._centerTouchLocation.x=_21.x;
this._centerTouchLocation.y=_21.y;
this._initialFingerSpacing=this._getFingerSpacing(_1f);
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
},_touchEndTapHandler:function(_22){
var _23=_22.touches;
if(_23.length==0){
if(this._oneFingerTouch&&!this._tapCancel){
this._oneFingerTouch=false;
setTimeout(_1.hitch(this,function(){
if(!this._doubleTapPerformed){
var dx=(_22.changedTouches[0].pageX-this._lastTap.x);
var dy=(_22.changedTouches[0].pageY-this._lastTap.y);
var _24=dx*dx+dy*dy;
if(_24<this._tapThreshold){
this._singleTapHandler(_22);
}
}
}),350);
}
this._tapCancel=false;
}
},_touchEndHandler:function(_25){
_3.stop(_25);
var _26=_25.touches;
if(_26.length==0){
if(this._touchMoveListener){
_4.disconnect(this._touchMoveListener);
this._touchMoveListener=null;
}
if(this._touchEndListener){
_4.disconnect(this._touchEndListener);
this._touchEndListener=null;
}
}else{
var _27=this._getTouchBarycenter(_25);
var _28=this._map.screenCoordsToMapCoords(_27.x,_27.y);
this._centerTouchLocation.x=_28.x;
this._centerTouchLocation.y=_28.y;
}
},_singleTapHandler:function(_29){
var _2a=this._getFeatureFromTouchEvent(_29);
if(_2a){
_2a._onclickHandler(_29);
}else{
for(var _2b in this._map.mapObj.features){
this._map.mapObj.features[_2b].select(false);
}
this._map.onFeatureClick(null);
}
},_touchMoveHandler:function(_2c){
_3.stop(_2c);
if(!this._tapCancel){
var dx=(_2c.touches[0].pageX-this._lastTap.x),dy=(_2c.touches[0].pageY-this._lastTap.y);
var _2d=dx*dx+dy*dy;
if(_2d>this._tapThreshold){
this._tapCancel=true;
}
}
var _2e=this._getTouchBarycenter(_2c);
var _2f=this._map.screenCoordsToMapCoords(_2e.x,_2e.y),_30=_2f.x-this._centerTouchLocation.x,_31=_2f.y-this._centerTouchLocation.y;
var _32=1;
var _33=_2c.touches;
if(_33.length>=2){
var _34=this._getFingerSpacing(_2c);
_32=_34/this._initialFingerSpacing;
this._map.setMapScale(this._initialScale*_32);
}
var _35=this._map.getMapCenter();
this._map.setMapCenter(_35.x-_30,_35.y-_31);
}});
});
