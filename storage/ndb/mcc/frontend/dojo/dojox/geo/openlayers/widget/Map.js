//>>built
define("dojox/geo/openlayers/widget/Map",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/html","dojo/query","dijit/_Widget","dojox/geo/openlayers/Map","dojox/geo/openlayers/Layer","dojox/geo/openlayers/GfxLayer"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.geo.openlayers.widget.Map",_6,{baseLayerType:dojox.geo.openlayers.BaseLayerType.OSM,initialLocation:null,touchHandler:false,map:null,startup:function(){
this.inherited(arguments);
this.map.initialFit({initialLocation:this.initialLocation});
},buildRendering:function(){
this.inherited(arguments);
var _a=this.domNode;
var _b=new _7(_a,{baseLayerType:this.baseLayerType,touchHandler:this.touchHandler});
this.map=_b;
this._makeLayers();
},_makeLayers:function(){
var n=this.domNode;
var _c=_5("> .layer",n);
_3.forEach(_c,function(l){
var _d=l.getAttribute("type");
var _e=l.getAttribute("name");
var _f="dojox.geo.openlayers."+_d;
var p=_1.getObject(_f);
if(p){
var _10=new p(_e,{});
if(_10){
this.map.addLayer(_10);
}
}
},this);
},resize:function(b){
var olm=this.map.getOLMap();
var box;
switch(arguments.length){
case 0:
break;
case 1:
box=_1.mixin({},b);
_1.marginBox(olm.div,box);
break;
case 2:
box={w:arguments[0],h:arguments[1]};
_1.marginBox(olm.div,box);
break;
}
olm.updateSize();
}});
});
