//>>built
define("dojox/geo/openlayers/Layer",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/sniff"],function(_1,_2,_3,_4,_5){
return _2("dojox.geo.openlayers.Layer",null,{constructor:function(_6,_7){
var ol=_7?_7.olLayer:null;
if(!ol){
ol=_3.delegate(new OpenLayers.Layer(_6,_7));
}
this.olLayer=ol;
this._features=null;
this.olLayer.events.register("moveend",this,_3.hitch(this,this.moveTo));
},renderFeature:function(f){
f.render();
},getDojoMap:function(){
return this.dojoMap;
},addFeature:function(f){
if(_3.isArray(f)){
_4.forEach(f,function(_8){
this.addFeature(_8);
},this);
return;
}
if(this._features==null){
this._features=[];
}
this._features.push(f);
f._setLayer(this);
},removeFeature:function(f){
var ft=this._features;
if(ft==null){
return;
}
if(f instanceof Array){
f=f.slice(0);
_4.forEach(f,function(_9){
this.removeFeature(_9);
},this);
return;
}
var i=_4.indexOf(ft,f);
if(i!=-1){
ft.splice(i,1);
}
f._setLayer(null);
f.remove();
},removeFeatureAt:function(_a){
var ft=this._features;
var f=ft[_a];
if(!f){
return;
}
ft.splice(_a,1);
f._setLayer(null);
f.remove();
},getFeatures:function(){
return this._features;
},getFeatureAt:function(i){
if(this._features==null){
return undefined;
}
return this._features[i];
},getFeatureCount:function(){
if(this._features==null){
return 0;
}
return this._features.length;
},clear:function(){
var fa=this.getFeatures();
this.removeFeature(fa);
},moveTo:function(_b){
if(_b.zoomChanged){
if(this._features==null){
return;
}
_4.forEach(this._features,function(f){
this.renderFeature(f);
},this);
}
},redraw:function(){
if(_5.isIE){
setTimeout(_3.hitch(this,function(){
this.olLayer.redraw();
},0));
}else{
this.olLayer.redraw();
}
},added:function(){
}});
});
