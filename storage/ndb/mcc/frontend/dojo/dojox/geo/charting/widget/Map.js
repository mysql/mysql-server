//>>built
define("dojox/geo/charting/widget/Map",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/html","dojo/dom-geometry","dijit/_Widget","../Map"],function(_1,_2,_3,_4,_5,_6,_7){
return _3("dojox.geo.charting.widget.Map",_6,{shapeData:"",dataStore:null,dataBindingAttribute:"",dataBindingValueFunction:null,markerData:"",series:"",adjustMapCenterOnResize:null,adjustMapScaleOnResize:null,animateOnResize:null,onFeatureClick:null,onFeatureOver:null,enableMouseSupport:null,enableTouchSupport:null,enableMouseZoom:null,enableMousePan:null,enableKeyboardSupport:false,showTooltips:false,enableFeatureZoom:null,colorAnimationDuration:0,mouseClickThreshold:2,_mouseInteractionSupport:null,_touchInteractionSupport:null,_keyboardInteractionSupport:null,constructor:function(_8,_9){
this.map=null;
},startup:function(){
this.inherited(arguments);
if(this.map){
this.map.fitToMapContents();
}
},postMixInProperties:function(){
this.inherited(arguments);
},create:function(_a,_b){
this.inherited(arguments);
},getInnerMap:function(){
return this.map;
},buildRendering:function(){
this.inherited(arguments);
if(this.shapeData){
this.map=new _7(this.domNode,this.shapeData);
if(this.markerData&&(this.markerData.length>0)){
this.map.setMarkerData(this.markerData);
}
if(this.dataStore){
if(this.dataBindingValueFunction){
this.map.setDataBindingValueFunction(this.dataBindingValueFunction);
}
this.map.setDataStore(this.dataStore,this.dataBindingAttribute);
}
if(this.series&&(this.series.length>0)){
this.map.addSeries(this.series);
}
if(this.onFeatureClick){
this.map.onFeatureClick=this.onFeatureClick;
}
if(this.onFeatureOver){
this.map.onFeatureOver=this.onFeatureOver;
}
if(this.enableMouseSupport){
if(!dojox.geo.charting.MouseInteractionSupport){
throw Error("Can't find dojox.geo.charting.MouseInteractionSupport. Didn't you forget to dojo"+".require() it?");
}
var _c={};
_c.enablePan=this.enableMousePan;
_c.enableZoom=this.enableMouseZoom;
_c.mouseClickThreshold=this.mouseClickThreshold;
this._mouseInteractionSupport=new dojox.geo.charting.MouseInteractionSupport(this.map,_c);
this._mouseInteractionSupport.connect();
}
if(this.enableTouchSupport){
if(!dojox.geo.charting.TouchInteractionSupport){
throw Error("Can't find dojox.geo.charting.TouchInteractionSupport. Didn't you forget to dojo"+".require() it?");
}
this._touchInteractionSupport=new dojox.geo.charting.TouchInteractionSupport(this.map,{});
this._touchInteractionSupport.connect();
}
if(this.enableKeyboardSupport){
if(!dojox.geo.charting.KeyboardInteractionSupport){
throw Error("Can't find dojox.geo.charting.KeyboardInteractionSupport. Didn't you forget to dojo"+".require() it?");
}
this._keyboardInteractionSupport=new dojox.geo.charting.KeyboardInteractionSupport(this.map,{});
this._keyboardInteractionSupport.connect();
}
this.map.showTooltips=this.showTooltips;
this.map.enableFeatureZoom=this.enableFeatureZoom;
this.map.colorAnimationDuration=this.colorAnimationDuration;
}
},resize:function(b,_d){
var _e;
switch(arguments.length){
case 0:
break;
case 1:
_e=_2.mixin({},b);
_5.setMarginBox(this.domNode,_e);
break;
case 2:
_e={w:arguments[0],h:arguments[1]};
_5.setMarginBox(this.domNode,_e);
break;
}
if(this.map){
this.map.resize(this.adjustMapCenterOnResize,this.adjustMapScaleOnResize,this.animateOnResize);
}
}});
});
