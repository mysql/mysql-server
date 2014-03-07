//>>built
define("dojox/geo/charting/_Marker",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/sniff","./_base"],function(_1,_2,_3,_4){
return _3("dojox.geo.charting._Marker",null,{_needTooltipRefresh:null,_map:null,constructor:function(_5,_6){
this._map=_6;
var _7=_6.mapObj;
this.features=_7.features;
this.markerData=_5;
_needTooltipRefresh=false;
},show:function(_8,_9){
this.currentFeature=this.features[_8];
if(this._map.showTooltips&&this.currentFeature){
this.markerText=this.currentFeature.markerText||this.markerData[_8]||_8;
dojox.geo.charting.showTooltip(this.markerText,this.currentFeature.shape,["before"]);
}
this._needTooltipRefresh=false;
},hide:function(){
if(this._map.showTooltips&&this.currentFeature){
dojox.geo.charting.hideTooltip(this.currentFeature.shape);
}
this._needTooltipRefresh=false;
},_getGroupBoundingBox:function(_a){
var _b=_a.children;
var _c=_b[0];
var _d=_c.getBoundingBox();
this._arround=_1.clone(_d);
_2.forEach(_b,function(_e){
var _f=_e.getBoundingBox();
this._arround.x=Math.min(this._arround.x,_f.x);
this._arround.y=Math.min(this._arround.y,_f.y);
},this);
},_toWindowCoords:function(_10,_11,_12){
var _13=(_10.x-this.topLeft[0])*this.scale;
var _14=(_10.y-this.topLeft[1])*this.scale;
if(_4("ff")==3.5){
_10.x=_11.x;
_10.y=_11.y;
}else{
if(_4("chrome")){
_10.x=_12.x+_13;
_10.y=_12.y+_14;
}else{
_10.x=_11.x+_13;
_10.y=_11.y+_14;
}
}
_10.width=(this.currentFeature._bbox[2])*this.scale;
_10.height=(this.currentFeature._bbox[3])*this.scale;
_10.x+=_10.width/6;
_10.y+=_10.height/4;
}});
});
