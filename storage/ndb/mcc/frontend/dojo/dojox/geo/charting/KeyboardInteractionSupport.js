//>>built
define("dojox/geo/charting/KeyboardInteractionSupport",["dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/connect","dojo/_base/html","dojo/dom","dojox/lang/functional","dojo/keys"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.geo.charting.KeyboardInteractionSupport",null,{_map:null,_zoomEnabled:false,constructor:function(_9,_a){
this._map=_9;
if(_a){
this._zoomEnabled=_a.enableZoom;
}
},connect:function(){
var _b=_6.byId(this._map.container);
_5.attr(_b,{tabindex:0,role:"presentation","aria-label":"map"});
this._keydownListener=_4.connect(_b,"keydown",this,"keydownHandler");
this._onFocusListener=_4.connect(_b,"focus",this,"onFocus");
this._onBlurListener=_4.connect(_b,"blur",this,"onBlur");
},disconnect:function(){
_4.disconnect(this._keydownListener);
this._keydownListener=null;
_4.disconnect(this._onFocusListener);
this._onFocusListener=null;
_4.disconnect(this._onBlurListener);
this._onBlurListener=null;
},keydownHandler:function(e){
switch(e.keyCode){
case _8.LEFT_ARROW:
this._directTo(-1,-1,1,-1);
break;
case _8.RIGHT_ARROW:
this._directTo(-1,-1,-1,1);
break;
case _8.UP_ARROW:
this._directTo(1,-1,-1,-1);
break;
case _8.DOWN_ARROW:
this._directTo(-1,1,-1,-1);
break;
case _8.SPACE:
if(this._map.selectedFeature&&!this._map.selectedFeature._isZoomIn&&this._zoomEnabled){
this._map.selectedFeature._zoomIn();
}
break;
case _8.ESCAPE:
if(this._map.selectedFeature&&this._map.selectedFeature._isZoomIn&&this._zoomEnabled){
this._map.selectedFeature._zoomOut();
}
break;
default:
return;
}
_3.stop(e);
},onFocus:function(e){
if(this._map.selectedFeature||this._map.focused){
return;
}
this._map.focused=true;
var _c,_d=false;
if(this._map.lastSelectedFeature){
_c=this._map.lastSelectedFeature;
}else{
var _e=this._map.getMapCenter(),_f=Infinity;
_7.forIn(this._map.mapObj.features,function(_10){
var _11=Math.sqrt(Math.pow(_10._center[0]-_e.x,2)+Math.pow(_10._center[1]-_e.y,2));
if(_11<_f){
_f=_11;
_c=_10;
}
});
_d=true;
}
if(_c){
if(_d){
_c._onclickHandler(null);
}else{
}
this._map.mapObj.marker.show(_c.id);
}
},onBlur:function(){
this._map.lastSelectedFeature=this._map.selectedFeature;
},_directTo:function(up,_12,_13,_14){
var _15=this._map.selectedFeature,_16=_15._center[0],_17=_15._center[1],_18=Infinity,_19=null;
_7.forIn(this._map.mapObj.features,function(_1a){
var _1b=Math.abs(_16-_1a._center[0]),_1c=Math.abs(_17-_1a._center[1]),_1d=_1b+_1c;
if((up-_12)*(_17-_1a._center[1])>0){
if(_1b<_1c&&_18>_1d){
_18=_1d;
_19=_1a;
}
}
if((_13-_14)*(_16-_1a._center[0])>0){
if(_1b>_1c&&_18>_1d){
_18=_1d;
_19=_1a;
}
}
});
if(_19){
this._map.mapObj.marker.hide();
_19._onclickHandler(null);
this._map.mapObj.marker.show(_19.id);
}
}});
});
