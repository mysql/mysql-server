//>>built
define("dojox/geo/openlayers/_base",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.geo.openlayers",true);
_2.BaseLayerType={OSM:"OSM",Transport:"OSM.Transport",WMS:"WMS",GOOGLE:"Google",VIRTUAL_EARTH:"VirtualEarth",BING:"VirtualEarth",YAHOO:"Yahoo",ARCGIS:"ArcGIS"};
_2.EPSG4326=new OpenLayers.Projection("EPSG:4326");
var re=/^\s*(\d{1,3})[D°]\s*(\d{1,2})[M']\s*(\d{1,2}\.?\d*)\s*(S|"|'')\s*([NSEWnsew]{0,1})\s*$/i;
_2.parseDMS=function(v,_3){
var _4=re.exec(v);
if(_4==null||_4.length<5){
return parseFloat(v);
}
var d=parseFloat(_4[1]);
var m=parseFloat(_4[2]);
var s=parseFloat(_4[3]);
var _5=_4[5];
if(_3){
var lc=_5.toLowerCase();
var dd=d+(m+s/60)/60;
if(lc=="w"||lc=="s"){
dd=-dd;
}
return dd;
}
return [d,m,s,_5];
};
return _2;
});
