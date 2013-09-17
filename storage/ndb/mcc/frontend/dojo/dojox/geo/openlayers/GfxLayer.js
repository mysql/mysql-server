//>>built
define("dojox/geo/openlayers/GfxLayer",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/html","dojox/gfx","dojox/gfx/_base","dojox/gfx/shape","dojox/gfx/path","dojox/gfx/matrix","dojox/geo/openlayers/Feature","dojox/geo/openlayers/Layer"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.geo.openlayers.GfxLayer",_b,{_viewport:null,constructor:function(_c,_d){
var s=dojox.gfx.createSurface(this.olLayer.div,100,100);
this._surface=s;
var vp;
if(_d&&_d.viewport){
vp=_d.viewport;
}else{
vp=s.createGroup();
}
this.setViewport(vp);
_1.connect(this.olLayer,"onMapResize",this,"onMapResize");
this.olLayer.getDataExtent=this.getDataExtent;
},getViewport:function(){
return this._viewport;
},setViewport:function(g){
if(this._viewport){
this._viewport.removeShape();
}
this._viewport=g;
this._surface.add(g);
},onMapResize:function(){
this._surfaceSize();
},setMap:function(_e){
this.inherited(arguments);
this._surfaceSize();
},getDataExtent:function(){
var _f=this._surface.getDimensions();
return _f;
},getSurface:function(){
return this._surface;
},_surfaceSize:function(){
var s=this.olLayer.map.getSize();
this._surface.setDimensions(s.w,s.h);
},moveTo:function(_10){
var s=_1.style(this.olLayer.map.layerContainerDiv);
var _11=parseInt(s.left);
var top=parseInt(s.top);
if(_10.zoomChanged||_11||top){
var d=this.olLayer.div;
_1.style(d,{left:-_11+"px",top:-top+"px"});
if(this._features==null){
return;
}
var vp=this.getViewport();
vp.setTransform(_9.translate(_11,top));
this.inherited(arguments);
}
},added:function(){
this.inherited(arguments);
this._surfaceSize();
}});
});
