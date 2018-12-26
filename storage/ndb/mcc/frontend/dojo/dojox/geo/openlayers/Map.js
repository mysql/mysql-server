//>>built
define("dojox/geo/openlayers/Map",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/json","dojo/_base/html","dojox/main","dojox/geo/openlayers/TouchInteractionSupport","dojox/geo/openlayers/Layer","dojox/geo/openlayers/Patch"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_1.experimental("dojox.geo.openlayers.Map");
_3.getObject("geo.openlayers",true,_7);
_7.geo.openlayers.BaseLayerType={OSM:"OSM",WMS:"WMS",GOOGLE:"Google",VIRTUAL_EARTH:"VirtualEarth",BING:"VirtualEarth",YAHOO:"Yahoo",ARCGIS:"ArcGIS"};
_7.geo.openlayers.EPSG4326=new OpenLayers.Projection("EPSG:4326");
var re=/^\s*(\d{1,3})[D°]\s*(\d{1,2})[M']\s*(\d{1,2}\.?\d*)\s*(S|"|'')\s*([NSEWnsew]{0,1})\s*$/i;
_7.geo.openlayers.parseDMS=function(v,_b){
var _c=re.exec(v);
if(_c==null||_c.length<5){
return parseFloat(v);
}
var d=parseFloat(_c[1]);
var m=parseFloat(_c[2]);
var s=parseFloat(_c[3]);
var _d=_c[5];
if(_b){
var lc=_d.toLowerCase();
var dd=d+(m+s/60)/60;
if(lc=="w"||lc=="s"){
dd=-dd;
}
return dd;
}
return [d,m,s,_d];
};
_a.patchGFX();
return _2("dojox.geo.openlayers.Map",null,{olMap:null,_tp:null,constructor:function(_e,_f){
if(!_f){
_f={};
}
_e=_6.byId(_e);
this._tp={x:0,y:0};
var _10=_f.openLayersMapOptions;
if(!_10){
_10={controls:[new OpenLayers.Control.ScaleLine({maxWidth:200}),new OpenLayers.Control.Navigation()]};
}
if(_f.accessible){
var kbd=new OpenLayers.Control.KeyboardDefaults();
if(!_10.controls){
_10.controls=[];
}
_10.controls.push(kbd);
}
var _11=_f.baseLayerType;
if(!_11){
_11=_7.geo.openlayers.BaseLayerType.OSM;
}
_6.style(_e,{width:"100%",height:"100%",dir:"ltr"});
var map=new OpenLayers.Map(_e,_10);
this.olMap=map;
this._layerDictionary={olLayers:[],layers:[]};
if(_f.touchHandler){
this._touchControl=new _8(map);
}
var _12=this._createBaseLayer(_f);
this.addLayer(_12);
this.initialFit(_f);
},initialFit:function(_13){
var o=_13.initialLocation;
if(!o){
o=[-160,70,160,-70];
}
this.fitTo(o);
},setBaseLayerType:function(_14){
if(_14==this.baseLayerType){
return null;
}
var o=null;
if(typeof _14=="string"){
o={baseLayerName:_14,baseLayerType:_14};
this.baseLayerType=_14;
}else{
if(typeof _14=="object"){
o=_14;
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
var _15=!!oc&&!!olm.baseLayer&&!!olm.baseLayer.map;
if(_15){
var _16=olm.getProjectionObject();
if(_16!=null){
oc=oc.transform(_16,_7.geo.openlayers.EPSG4326);
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
if(_15){
_16=olm.getProjectionObject();
if(_16!=null){
oc=oc.transform(_7.geo.openlayers.EPSG4326,_16);
}
olm.setCenter(oc,ob);
}
}
}
return bl;
},getBaseLayerType:function(){
return this.baseLayerType;
},getScale:function(_17){
var _18;
var om=this.olMap;
if(_17){
var _19=om.getUnits();
if(!_19){
return null;
}
var _1a=OpenLayers.INCHES_PER_UNIT;
_18=(om.getGeodesicPixelSize().w||0.000001)*_1a["km"]*OpenLayers.DOTS_PER_INCH;
}else{
_18=om.getScale();
}
return _18;
},getOLMap:function(){
return this.olMap;
},_createBaseLayer:function(_1b){
var _1c=null;
var _1d=_1b.baseLayerType;
var url=_1b.baseLayerUrl;
var _1e=_1b.baseLayerName;
var _1f=_1b.baseLayerOptions;
if(!_1e){
_1e=_1d;
}
if(!_1f){
_1f={};
}
switch(_1d){
case _7.geo.openlayers.BaseLayerType.OSM:
_1f.transitionEffect="resize";
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.OSM(_1e,url,_1f)});
break;
case _7.geo.openlayers.BaseLayerType.WMS:
if(!url){
url="http://labs.metacarta.com/wms/vmap0";
if(!_1f.layers){
_1f.layers="basic";
}
}
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.WMS(_1e,url,_1f,{transitionEffect:"resize"})});
break;
case _7.geo.openlayers.BaseLayerType.GOOGLE:
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.Google(_1e,_1f)});
break;
case _7.geo.openlayers.BaseLayerType.VIRTUAL_EARTH:
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.VirtualEarth(_1e,_1f)});
break;
case _7.geo.openlayers.BaseLayerType.YAHOO:
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.Yahoo(_1e,_1f)});
break;
case _7.geo.openlayers.BaseLayerType.ARCGIS:
if(!url){
url="http://server.arcgisonline.com/ArcGIS/rest/services/ESRI_StreetMap_World_2D/MapServer/export";
}
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.ArcGIS93Rest(_1e,url,_1f,{})});
break;
}
if(_1c==null){
if(_1d instanceof OpenLayers.Layer){
_1c=_1d;
}else{
_1f.transitionEffect="resize";
_1c=new _9(_1e,{olLayer:new OpenLayers.Layer.OSM(_1e,url,_1f)});
this.baseLayerType=_7.geo.openlayers.BaseLayerType.OSM;
}
}
return _1c;
},removeLayer:function(_20){
var om=this.olMap;
var i=_4.indexOf(this._layerDictionary.layers,_20);
if(i>0){
this._layerDictionary.layers.splice(i,1);
}
var oll=_20.olLayer;
var j=_4.indexOf(this._layerDictionary.olLayers,oll);
if(j>0){
this._layerDictionary.olLayers.splice(i,j);
}
om.removeLayer(oll,false);
},layerIndex:function(_21,_22){
var olm=this.olMap;
if(!_22){
return olm.getLayerIndex(_21.olLayer);
}
olm.setLayerIndex(_21.olLayer,_22);
this._layerDictionary.layers.sort(function(l1,l2){
return olm.getLayerIndex(l1.olLayer)-olm.getLayerIndex(l2.olLayer);
});
this._layerDictionary.olLayers.sort(function(l1,l2){
return olm.getLayerIndex(l1)-olm.getLayerIndex(l2);
});
return _22;
},addLayer:function(_23){
_23.dojoMap=this;
var om=this.olMap;
var ol=_23.olLayer;
this._layerDictionary.olLayers.push(ol);
this._layerDictionary.layers.push(_23);
om.addLayer(ol);
_23.added();
},_getLayer:function(ol){
var i=_4.indexOf(this._layerDictionary.olLayers,ol);
if(i!=-1){
return this._layerDictionary.layers[i];
}
return null;
},getLayer:function(_24,_25){
var om=this.olMap;
var ols=om.getBy("layers",_24,_25);
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
var _26=_7.geo.openlayers.EPSG4326;
if(o==null){
var c=this.transformXY(0,0,_26);
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
ul=this.transformXY(a[0],a[1],_26);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(a[2],a[3],_26);
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
ul=this.transformXY(p[0]-e,p[1]+e,_26);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(p[0]+e,p[1]-e,_26);
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
ul=this.transformXY(o[0],o[1],_26);
b.left=ul.x;
b.top=ul.y;
lr=this.transformXY(o[2],o[3],_26);
b.right=lr.x;
b.bottom=lr.y;
}
}
}
if(b!=null){
map.zoomToExtent(b,true);
}
},transform:function(p,_27,to){
return this.transformXY(p.x,p.y,_27,to);
},transformXY:function(x,y,_28,to){
var tp=this._tp;
tp.x=x;
tp.y=y;
if(!_28){
_28=_7.geo.openlayers.EPSG4326;
}
if(!to){
to=this.olMap.getProjectionObject();
}
tp=OpenLayers.Projection.transform(tp,_28,to);
return tp;
}});
});
