//>>built
define("dojox/geo/openlayers/JsonImport",["dojo/_base/declare","dojo/_base/xhr","dojo/_base/lang","dojo/_base/array","./LineString","./Collection","./GeometryFeature"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.geo.openlayers.JsonImport",null,{constructor:function(_8){
this._params=_8;
},loadData:function(){
var p=this._params;
_2.get({url:p.url,handleAs:"json",sync:true,load:_3.hitch(this,this._gotData),error:_3.hitch(this,this._loadError)});
},_gotData:function(_9){
var nf=this._params.nextFeature;
if(!_3.isFunction(nf)){
return;
}
var _a=_9.layerExtent;
var _b=_a[0];
var _c=_a[1];
var _d=_b+_a[2];
var _e=_c+_a[3];
var _f=_9.layerExtentLL;
var x1=_f[0];
var y1=_f[1];
var x2=x1+_f[2];
var y2=y1+_f[3];
var _10=x1;
var _11=y2;
var _12=x2;
var _13=y1;
var _14=_9.features;
for(var f in _14){
var o=_14[f];
var s=o["shape"];
var gf=null;
if(_3.isArray(s[0])){
var a=new Array();
_4.forEach(s,function(_15){
var ls=this._makeGeometry(_15,_b,_c,_d,_e,_10,_11,_12,_13);
a.push(ls);
},this);
var g=new _6(a);
gf=new _7(g);
nf.call(this,gf);
}else{
gf=this._makeFeature(s,_b,_c,_d,_e,_10,_11,_12,_13);
nf.call(this,gf);
}
}
var _16=this._params.complete;
if(_3.isFunction(_16)){
_16.call(this,_16);
}
},_makeGeometry:function(s,ulx,uly,lrx,lry,_17,_18,_19,_1a){
var a=[];
var k=0;
for(var i=0;i<s.length-1;i+=2){
var x=s[i];
var y=s[i+1];
k=(x-ulx)/(lrx-ulx);
var px=k*(_19-_17)+_17;
k=(y-uly)/(lry-uly);
var py=k*(_1a-_18)+_18;
a.push({x:px,y:py});
}
var ls=new _5(a);
return ls;
},_makeFeature:function(s,ulx,uly,lrx,lry,_1b,_1c,_1d,_1e){
var ls=this._makeGeometry(s,ulx,uly,lrx,lry,_1b,_1c,_1d,_1e);
var gf=new _7(ls);
return gf;
},_loadError:function(){
var f=this._params.error;
if(_3.isFunction(f)){
f.apply(this,parameters);
}
}});
});
