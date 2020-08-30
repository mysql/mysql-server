//>>built
define("dojox/geo/openlayers/Map",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/json","dojo/dom","dojo/dom-style","./_base","./TouchInteractionSupport","./Layer","./Patch"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
_1.experimental("dojox.geo.openlayers.Map");
_b.patchGFX();
return _2("dojox.geo.openlayers.Map",null,{olMap:null,_tp:null,constructor:function(_c,_d){
if(!_d){
_d={};
}
_c=_6.byId(_c);
this._tp={x:0,y:0};
var _e=_d.openLayersMapOptions;
if(!_e){
_e={controls:[new OpenLayers.Control.ScaleLine({maxWidth:200}),new OpenLayers.Control.Navigation()]};
}
if(_d.accessible){
var _f=new OpenLayers.Control.KeyboardDefaults();
if(!_e.controls){
_e.controls=[];
}
_e.controls.push(_f);
}
var _10=_d.baseLayerType;
if(!_10){
_10=_8.BaseLayerType.OSM;
}
var map=new OpenLayers.Map(_c,_e);
this.olMap=map;
this._layerDictionary={olLayers:[],layers:[]};
if(_d.touchHandler){
this._touchControl=new _9(map);
}
var _11=this._createBaseLayer(_d);
this.addLayer(_11);
this.initialFit(_d);
},initialFit:function(_12){
var o=_12.initialLocation;
if(!o){
o=[-160,70,160,-70];
}
this.fitTo(o);
},setBaseLayerType:function(_13){
if(_13==this.baseLayerType){
return null;
}
var o=null;
if(typeof _13=="string"){
o={baseLayerName:_13,baseLayerType:_13};
this.baseLayerType=_13;
}else{
if(typeof _13=="object"){
o=_13;
this.baseLayerType=o.baseLayerType;
}
}
var bl=null;
if(o!=null){
bl=this._createBaseLayer(o);
if(bl!=null){
var olm=this.olMap;
var ob=olm.getZoom();
var oc=olm.getCenter();
var _14=!!oc&&!!olm.baseLayer&&!!olm.baseLayer.map;
if(_14){
var _15=olm.getProjectionObject();
if(_15!=null){
oc=oc.transform(_15,_8.EPSG4326);
}
}
var old=olm.baseLayer;
if(old!=null){
var l=this._getLayer(old);
this.removeLayer(l);
}
if(bl!=null){
this.addLayer(bl);
}
if(_14){
_15=olm.getProjectionObject();
if(_15!=null){
oc=oc.transform(_8.EPSG4326,_15);
}
olm.setCenter(oc,ob);
}
}
}
return bl;
},getBaseLayerType:function(){
return this.baseLayerType;
},getScale:function(_16){
var _17=null;
var om=this.olMap;
if(_16){
var _18=om.getUnits();
if(!_18){
return null;
}
var _19=OpenLayers.INCHES_PER_UNIT;
_17=(om.getGeodesicPixelSize().w||0.000001)*_19["km"]*OpenLayers.DOTS_PER_INCH;
}else{
_17=om.getScale();
}
return _17;
},getOLMap:function(){
return this.olMap;
},_createBaseLayer:function(_1a){
var _1b=null;
var _1c=_1a.baseLayerType;
var url=_1a.baseLayerUrl;
var _1d=_1a.baseLayerName;
var _1e=_1a.baseLayerOptions;
if(!_1d){
_1d=_1c;
}
if(!_1e){
_1e={};
}
switch(_1c){
case _8.BaseLayerType.OSM:
_1e.transitionEffect="resize";
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.OSM(_1d,url,_1e)});
break;
case _8.BaseLayerType.Transport:
_1e.transitionEffect="resize";
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.OSM.TransportMap(_1d,url,_1e)});
break;
case _8.BaseLayerType.WMS:
if(!url){
url="http://labs.metacarta.com/wms/vmap0";
if(!_1e.layers){
_1e.layers="basic";
}
}
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.WMS(_1d,url,_1e,{transitionEffect:"resize"})});
break;
case _8.BaseLayerType.GOOGLE:
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.Google(_1d,_1e)});
break;
case _8.BaseLayerType.VIRTUAL_EARTH:
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.VirtualEarth(_1d,_1e)});
break;
case _8.BaseLayerType.YAHOO:
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.Yahoo(_1d,_1e)});
break;
case _8.BaseLayerType.ARCGIS:
if(!url){
url="http://server.arcgisonline.com/ArcGIS/rest/services/ESRI_StreetMap_World_2D/MapServer/export";
}
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.ArcGIS93Rest(_1d,url,_1e,{})});
break;
}
if(_1b==null){
if(_1c instanceof OpenLayers.Layer){
_1b=_1c;
}else{
_1e.transitionEffect="resize";
_1b=new _a(_1d,{olLayer:new OpenLayers.Layer.OSM(_1d,url,_1e)});
this.baseLayerType=_8.BaseLayerType.OSM;
}
}
return _1b;
},removeLayer:function(_1f){
var om=this.olMap;
var i=_4.indexOf(this._layerDictionary.layers,_1f);
if(i>0){
this._layerDictionary.layers.splice(i,1);
}
var oll=_1f.olLayer;
var j=_4.indexOf(this._layerDictionary.olLayers,oll);
if(j>0){
this._layerDictionary.olLayers.splice(i,j);
}
om.removeLayer(oll,false);
},layerIndex:function(_20,_21){
var olm=this.olMap;
if(!_21){
return olm.getLayerIndex(_20.olLayer);
}
olm.setLayerIndex(_20.olLayer,_21);
this._layerDictionary.layers.sort(function(l1,l2){
return olm.getLayerIndex(l1.olLayer)-olm.getLayerIndex(l2.olLayer);
});
this._layerDictionary.olLayers.sort(function(l1,l2){
return olm.getLayerIndex(l1)-olm.getLayerIndex(l2);
});
return _21;
},addLayer:function(_22){
_22.dojoMap=this;
var om=this.olMap;
var ol=_22.olLayer;
this._layerDictionary.olLayers.push(ol);
this._layerDictionary.layers.push(_22);
om.addLayer(ol);
_22.added();
},_getLayer:function(ol){
var i=_4.indexOf(this._layerDictionary.olLayers,ol);
if(i!=-1){
return this._layerDictionary.layers[i];
}
return null;
},getLayer:function(_23,_24){
var om=this.olMap;
var ols=om.getBy("layers",_23,_24);
var ret=new Array();
_4.forEach(ols,function(ol){
ret.push(this._getLayer(ol));
},this);
return ret;
},getLayerCount:function(){
var om=this.olMap;
if(om.layers==null){
return 0;
}
return om.layers.length;
},fitTo:function(o){
var map=this.olMap;
var _25=_8.EPSG4326;
if(o==null){
var c=this.transformXY(0,0,_25);
map.setCenter(new OpenLayers.LonLat(c.x,c.y));
return;
}
var b=null;
if(typeof o=="string"){
var j=_5.fromJson(o);
}else{
j=o;
}
var ul;
var lr;
if(j.hasOwnProperty("bounds")){
var a=j.bounds;
b=new OpenLayers.Bounds();
ul=this.transformXY(a[0],a[1],_25);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(a[2],a[3],_25);
b.right=lr.x;
b.bottom=lr.y;
}
if(b==null){
if(j.hasOwnProperty("position")){
var p=j.position;
var e=j.hasOwnProperty("extent")?j.extent:1;
if(typeof e=="string"){
e=parseFloat(e);
}
b=new OpenLayers.Bounds();
ul=this.transformXY(p[0]-e,p[1]+e,_25);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(p[0]+e,p[1]-e,_25);
b.right=lr.x;
b.bottom=lr.y;
}
}
if(b==null){
if(o.length==4){
b=new OpenLayers.Bounds();
if(false){
b.left=o[0];
b.top=o[1];
b.right=o[2];
b.bottom=o[3];
}else{
ul=this.transformXY(o[0],o[1],_25);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(o[2],o[3],_25);
b.right=lr.x;
b.bottom=lr.y;
}
}
}
if(b!=null){
map.zoomToExtent(b,true);
}
},transform:function(p,_26,to){
return this.transformXY(p.x,p.y,_26,to);
},transformXY:function(x,y,_27,to){
var tp=this._tp;
tp.x=x;
tp.y=y;
if(!_27){
_27=_8.EPSG4326;
}
if(!to){
to=this.olMap.getProjectionObject();
}
tp=OpenLayers.Projection.transform(tp,_27,to);
return tp;
}});
});
