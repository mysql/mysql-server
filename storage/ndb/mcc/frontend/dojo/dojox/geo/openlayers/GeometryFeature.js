//>>built
define("dojox/geo/openlayers/GeometryFeature",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojox/gfx/matrix","./Point","./LineString","./Collection","./Feature"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.geo.openlayers.GeometryFeature",_8,{constructor:function(_9){
this._geometry=_9;
this._shapeProperties={};
this._fill=null;
this._stroke=null;
},_createCollection:function(g){
var _a=this.getLayer();
var s=_a.getSurface();
var c=this.createShape(s,g);
var vp=_a.getViewport();
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
var _b=this.getShapeProperties();
s.setShape(_b);
_2.forEach(g.coordinates,function(_c){
if(_c instanceof _5){
this.renderPoint(_c);
}else{
if(_c instanceof _6){
this.renderLineString(_c);
}else{
if(_c instanceof _7){
this.renderCollection(_c);
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
if(g instanceof _5){
this.renderPoint(g);
}else{
if(g instanceof _6){
this.renderLineString(g);
}else{
if(g instanceof _7){
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
var _d=null;
if(g instanceof _5){
_d=s.createCircle();
}else{
if(g instanceof _6){
_d=s.createPolyline();
}else{
if(g instanceof _7){
var _e=s.createGroup();
_2.forEach(g.coordinates,function(_f){
var shp=this.createShape(s,_f);
_e.add(shp);
},this);
_d=_e;
}else{
throw new Error();
}
}
}
return _d;
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
var _10=this.getLayer();
var s=_10.getSurface();
var c=this.createShape(s,g);
var vp=_10.getViewport();
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
var _11=this.getLayer();
var map=_11.getDojoMap();
s=this._getPointShape(g);
var _12=_3.mixin({},this._defaults.pointShape);
_12=_3.mixin(_12,this.getShapeProperties());
s.setShape(_12);
var _13=this.getCoordinateSystem();
var p=map.transform(g.coordinates,_13);
var a=this._getLocalXY(p);
var cx=a[0];
var cy=a[1];
var tr=_11.getViewport().getTransform();
if(tr){
s.setTransform(_4.translate(cx-tr.dx,cy-tr.dy));
}
this._applyStyle(g);
},_createLineString:function(g){
var _14=this.getLayer();
var s=_14._surface;
var _15=this.createShape(s,g);
var vp=_14.getViewport();
vp.add(_15);
g.shape=_15;
return _15;
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
var _16=this.getLayer();
var map=_16.getDojoMap();
var lss=this._getLineStringShape(g);
var _17=this.getCoordinateSystem();
var _18=new Array(g.coordinates.length);
var tr=_16.getViewport().getTransform();
_2.forEach(g.coordinates,function(c,i,_19){
var p=map.transform(c,_17);
var a=this._getLocalXY(p);
if(tr){
a[0]-=tr.dx;
a[1]-=tr.dy;
}
_18[i]={x:a[0],y:a[1]};
},this);
var _1a=_3.mixin({},this._defaults.lineStringShape);
_1a=_3.mixin(_1a,this.getShapeProperties());
_1a=_3.mixin(_1a,{points:_18});
lss.setShape(_1a);
this._applyStyle(g);
},_applyStyle:function(g){
if(!g||!g.shape){
return;
}
var f=this.getFill();
var _1b;
if(!f||_3.isString(f)||_3.isArray(f)){
_1b=f;
}else{
_1b=_3.mixin({},this._defaults.fill);
_1b=_3.mixin(_1b,f);
}
var s=this.getStroke();
var _1c;
if(!s||_3.isString(s)||_3.isArray(s)){
_1c=s;
}else{
_1c=_3.mixin({},this._defaults.stroke);
_1c=_3.mixin(_1c,s);
}
this._applyRecusiveStyle(g,_1c,_1b);
},_applyRecusiveStyle:function(g,_1d,_1e){
var shp=g.shape;
if(shp.setFill){
shp.setFill(_1e);
}
if(shp.setStroke){
shp.setStroke(_1d);
}
if(g instanceof _7){
_2.forEach(g.coordinates,function(i){
this._applyRecusiveStyle(i,_1d,_1e);
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
if(g instanceof _7){
_2.forEach(g.coordinates,function(i){
this.remove(i);
},this);
}
},_defaults:{fill:null,stroke:null,pointShape:{r:30},lineStringShape:null}});
});
