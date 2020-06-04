//>>built
define("dojox/geo/openlayers/GfxLayer",["dojo/_base/declare","dojo/_base/connect","dojo/dom-style","dojox/gfx","dojox/gfx/matrix","./Feature","./Layer"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.geo.openlayers.GfxLayer",_7,{_viewport:null,constructor:function(_8,_9){
var s=_4.createSurface(this.olLayer.div,100,100);
this._surface=s;
var vp;
if(_9&&_9.viewport){
vp=_9.viewport;
}else{
vp=s.createGroup();
}
this.setViewport(vp);
_2.connect(this.olLayer,"onMapResize",this,"onMapResize");
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
},setMap:function(_a){
this.inherited(arguments);
this._surfaceSize();
},getDataExtent:function(){
var _b=this._surface.getDimensions();
return _b;
},getSurface:function(){
return this._surface;
},_surfaceSize:function(){
var s=this.olLayer.map.getSize();
this._surface.setDimensions(s.w,s.h);
},moveTo:function(_c){
var s=_3.get(this.olLayer.map.layerContainerDiv);
var _d=parseInt(s.left);
var _e=parseInt(s.top);
if(_c.zoomChanged||_d||_e){
var d=this.olLayer.div;
_3.set(d,{left:-_d+"px",top:-_e+"px"});
if(this._features==null){
return;
}
var vp=this.getViewport();
vp.setTransform(_5.translate(_d,_e));
this.inherited(arguments);
}
},added:function(){
this.inherited(arguments);
this._surfaceSize();
}});
});
