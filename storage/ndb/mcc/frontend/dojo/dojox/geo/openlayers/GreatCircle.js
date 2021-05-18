//>>built
define("dojox/geo/openlayers/GreatCircle",["dojo/_base/lang","./_base","./GeometryFeature"],function(_1,_2,_3){
var gc=_2.GreatCircle={toPointArray:function(p1,p2,_4){
var _5=p1.x;
var _6=p2.x;
var sl=Math.min(_5,_6);
var el=Math.max(_5,_6);
var _7=this.DEG2RAD;
var _8=p1.y*_7;
var _9=p1.x*_7;
var _a=p2.y*_7;
var _b=p2.x*_7;
if(Math.abs(_9-_b)<=this.TOLERANCE){
var l=Math.min(_9,_b);
_b=l+Math.PI;
}
if(Math.abs(_b-_9)==Math.PI){
if(_8+_a==0){
_a+=Math.PI/180000000;
}
}
var _c=sl*_7;
var _d=el*_7;
var _e=_4*_7;
var wp=[];
var k=0;
var _f=this.RAD2DEG;
while(_c<=_d){
lat=Math.atan((Math.sin(_8)*Math.cos(_a)*Math.sin(_c-_b)-Math.sin(_a)*Math.cos(_8)*Math.sin(_c-_9))/(Math.cos(_8)*Math.cos(_a)*Math.sin(_9-_b)));
var p={x:_c*_f,y:lat*_f};
wp[k++]=p;
if(_c<_d&&(_c+_e)>=_d){
_c=_d;
}else{
_c=_c+_e;
}
}
return wp;
},toLineString:function(p1,p2,_10){
var wp=this.toPointArray(p1,p2,_10);
var ls=new OpenLayers.Geometry.LineString(wp);
return ls;
},toGeometryFeature:function(p1,p2,_11){
var ls=this.toLineString(p1,p2,_11);
return new _3(ls);
},DEG2RAD:Math.PI/180,RAD2DEG:180/Math.PI,TOLERANCE:0.00001};
return gc;
});
