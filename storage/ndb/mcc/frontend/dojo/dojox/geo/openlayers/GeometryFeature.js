//>>built
define("dojox/geo/openlayers/GeometryFeature",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojox/gfx/matrix","dojox/geo/openlayers/Point","dojox/geo/openlayers/LineString","dojox/geo/openlayers/Collection","dojox/geo/openlayers/Feature"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.geo.openlayers.GeometryFeature",_9,{constructor:function(_a){
this._geometry=_a;
this._shapeProperties={};
this._fill=null;
this._stroke=null;
},_createCollection:function(g){
var _b=this.getLayer();
var s=_b.getSurface();
var c=this.createShape(s,g);
var vp=_b.getViewport();
vp.add(c);
return c;
},_getCollectionShape:function(g){
var s=g.shape;
if(s==null){
s=this._createCollection(g);
g.shape=s;
}
return s;
},renderCollection:function(g){
if(g==undefined){
g=this._geometry;
}
s=this._getCollectionShape(g);
var _c=this.getShapeProperties();
s.setShape(_c);
_3.forEach(g.coordinates,function(_d){
if(_d instanceof _6){
this.renderPoint(_d);
}else{
if(_d instanceof _7){
this.renderLineString(_d);
}else{
if(_d instanceof _8){
this.renderCollection(_d);
}else{
throw new Error();
}
}
}
},this);
this._applyStyle(g);
},render:function(g){
if(g==undefined){
g=this._geometry;
}
if(g instanceof _6){
this.renderPoint(g);
}else{
if(g instanceof _7){
this.renderLineString(g);
}else{
if(g instanceof _8){
this.renderCollection(g);
}else{
throw new Error();
}
}
}
},getShapeProperties:function(){
return this._shapeProperties;
},setShapeProperties:function(s){
this._shapeProperties=s;
return this;
},createShape:function(s,g){
if(!g){
g=this._geometry;
}
var _e=null;
if(g instanceof _6){
_e=s.createCircle();
}else{
if(g instanceof _7){
_e=s.createPolyline();
}else{
if(g instanceof _8){
var _f=s.createGroup();
_3.forEach(g.coordinates,function(_10){
var shp=this.createShape(s,_10);
_f.add(shp);
},this);
_e=_f;
}else{
throw new Error();
}
}
}
return _e;
},getShape:function(){
var g=this._geometry;
if(!g){
return null;
}
if(g.shape){
return g.shape;
}
this.render();
return g.shape;
},_createPoint:function(g){
var _11=this.getLayer();
var s=_11.getSurface();
var c=this.createShape(s,g);
var vp=_11.getViewport();
vp.add(c);
return c;
},_getPointShape:function(g){
var s=g.shape;
if(s==null){
s=this._createPoint(g);
g.shape=s;
}
return s;
},renderPoint:function(g){
if(g==undefined){
g=this._geometry;
}
var _12=this.getLayer();
var map=_12.getDojoMap();
s=this._getPointShape(g);
var _13=_4.mixin({},this._defaults.pointShape);
_13=_4.mixin(_13,this.getShapeProperties());
s.setShape(_13);
var _14=this.getCoordinateSystem();
var p=map.transform(g.coordinates,_14);
var a=this._getLocalXY(p);
var cx=a[0];
var cy=a[1];
var tr=_12.getViewport().getTransform();
if(tr){
s.setTransform(_5.translate(cx-tr.dx,cy-tr.dy));
}
this._applyStyle(g);
},_createLineString:function(g){
var _15=this.getLayer();
var s=_15._surface;
var _16=this.createShape(s,g);
var vp=_15.getViewport();
vp.add(_16);
g.shape=_16;
return _16;
},_getLineStringShape:function(g){
var s=g.shape;
if(s==null){
s=this._createLineString(g);
g.shape=s;
}
return s;
},renderLineString:function(g){
if(g==undefined){
g=this._geometry;
}
var _17=this.getLayer();
var map=_17.getDojoMap();
var lss=this._getLineStringShape(g);
var _18=this.getCoordinateSystem();
var _19=new Array(g.coordinates.length);
var tr=_17.getViewport().getTransform();
_3.forEach(g.coordinates,function(c,i,_1a){
var p=map.transform(c,_18);
var a=this._getLocalXY(p);
if(tr){
a[0]-=tr.dx;
a[1]-=tr.dy;
}
_19[i]={x:a[0],y:a[1]};
},this);
var _1b=_4.mixin({},this._defaults.lineStringShape);
_1b=_4.mixin(_1b,this.getShapeProperties());
_1b=_4.mixin(_1b,{points:_19});
lss.setShape(_1b);
this._applyStyle(g);
},_applyStyle:function(g){
if(!g||!g.shape){
return;
}
var f=this.getFill();
var _1c;
if(!f||_4.isString(f)||_4.isArray(f)){
_1c=f;
}else{
_1c=_4.mixin({},this._defaults.fill);
_1c=_4.mixin(_1c,f);
}
var s=this.getStroke();
var _1d;
if(!s||_4.isString(s)||_4.isArray(s)){
_1d=s;
}else{
_1d=_4.mixin({},this._defaults.stroke);
_1d=_4.mixin(_1d,s);
}
this._applyRecusiveStyle(g,_1d,_1c);
},_applyRecusiveStyle:function(g,_1e,_1f){
var shp=g.shape;
if(shp.setFill){
shp.setFill(_1f);
}
if(shp.setStroke){
shp.setStroke(_1e);
}
if(g instanceof _8){
_3.forEach(g.coordinates,function(i){
this._applyRecusiveStyle(i,_1e,_1f);
},this);
}
},setStroke:function(s){
this._stroke=s;
return this;
},getStroke:function(){
return this._stroke;
},setFill:function(f){
this._fill=f;
return this;
},getFill:function(){
return this._fill;
},remove:function(){
var g=this._geometry;
var shp=g.shape;
g.shape=null;
if(shp){
shp.removeShape();
}
if(g instanceof _8){
_3.forEach(g.coordinates,function(i){
this.remove(i);
},this);
}
},_defaults:{fill:null,stroke:null,pointShape:{r:30},lineStringShape:null}});
});
