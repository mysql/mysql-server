//>>built
define("dojox/geo/openlayers/widget/Map",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","dojo/query","dijit/_Widget","../_base","../Map","../Layer","../GfxLayer"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.geo.openlayers.widget.Map",_6,{baseLayerType:_7.BaseLayerType.OSM,initialLocation:null,touchHandler:false,map:null,startup:function(){
this.inherited(arguments);
this.map.initialFit({initialLocation:this.initialLocation});
},buildRendering:function(){
this.inherited(arguments);
var _b=this.domNode;
var _c=new _8(_b,{baseLayerType:this.baseLayerType,touchHandler:this.touchHandler});
this.map=_c;
this._makeLayers();
},_makeLayers:function(){
var n=this.domNode;
var _d=_5("> .layer",n);
_3.forEach(_d,function(l){
var _e=l.getAttribute("type");
var _f=l.getAttribute("name");
var cls="dojox.geo.openlayers."+_e;
var p=_1.getObject(cls);
if(p){
var _10=new p(_f,{});
if(_10){
this.map.addLayer(_10);
}
}
},this);
},resize:function(b,h){
var olm=this.map.getOLMap();
var box;
switch(arguments.length){
case 0:
break;
case 1:
box=_1.mixin({},b);
_4.setMarginBox(olm.div,box);
break;
case 2:
box={w:arguments[0],h:arguments[1]};
_4.setMarginBox(olm.div,box);
break;
}
olm.updateSize();
}});
});
