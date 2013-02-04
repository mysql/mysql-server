//>>built
define("dojox/geo/openlayers/GreatCircle",["dojo/_base/lang","dojox/geo/openlayers/GeometryFeature","dojox/geo/openlayers/Point","dojox/geo/openlayers/LineString"],function(_1,_2,_3,_4){
_1.getObject("geo.openlayers",true,dojox);
dojox.geo.openlayers.GreatCircle={toPointArray:function(p1,p2,_5){
var _6=p1.x;
var _7=p2.x;
var sl=Math.min(_6,_7);
var el=Math.max(_6,_7);
var _8=this.DEG2RAD;
var _9=p1.y*_8;
var _a=p1.x*_8;
var _b=p2.y*_8;
var _c=p2.x*_8;
if(Math.abs(_a-_c)<=this.TOLERANCE){
var l=Math.min(_a,_c);
_c=l+Math.PI;
}
if(Math.abs(_c-_a)==Math.PI){
if(_9+_b==0){
_b+=Math.PI/180000000;
}
}
var _d=sl*_8;
var _e=el*_8;
var _f=_5*_8;
var wp=[];
var k=0;
var r2d=this.RAD2DEG;
while(_d<=_e){
lat=Math.atan((Math.sin(_9)*Math.cos(_b)*Math.sin(_d-_c)-Math.sin(_b)*Math.cos(_9)*Math.sin(_d-_a))/(Math.cos(_9)*Math.cos(_b)*Math.sin(_a-_c)));
var p={x:_d*r2d,y:lat*r2d};
wp[k++]=p;
if(_d<_e&&(_d+_f)>=_e){
_d=_e;
}else{
_d=_d+_f;
}
}
return wp;
},toLineString:function(p1,p2,_10){
var wp=this.toPointArray(p1,p2,_10);
var ls=new OpenLayers.Geometry.LineString(wp);
return ls;
},toGeometryFeature:function(p1,p2,_11){
var ls=this.toLineString(p1,p2,_11);
return new _2(ls);
},DEG2RAD:Math.PI/180,RAD2DEG:180/Math.PI,TOLERANCE:0.00001};
return dojox.geo.openlayers.GreatCircle;
});
