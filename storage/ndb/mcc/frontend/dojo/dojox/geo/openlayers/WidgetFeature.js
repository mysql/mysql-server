//>>built
define("dojox/geo/openlayers/WidgetFeature",["dojo/_base/declare","dojo/dom-style","dojo/_base/lang","dijit/registry","./Feature"],function(_1,_2,_3,_4,_5){
return _1("dojox.geo.openlayers.WidgetFeature",_5,{_widget:null,_bbox:null,constructor:function(_6){
this._params=_6;
},setParameters:function(_7){
this._params=_7;
},getParameters:function(){
return this._params;
},_getWidget:function(){
var _8=this._params;
if((this._widget==null)&&(_8!=null)){
var w=null;
if(typeof (_8.createWidget)=="function"){
w=_8.createWidget.call(this);
}else{
if(_8.dojoType){
dojo["require"](_8.dojoType);
var c=_3.getObject(_8.dojoType);
w=new c(_8);
}else{
if(_8.dijitId){
w=_4.byId(_8.dijitId);
}else{
if(_8.widget){
w=_8.widget;
}
}
}
}
if(w!=null){
this._widget=w;
if(typeof (w.startup)=="function"){
w.startup();
}
var n=w.domNode;
if(n!=null){
_2.set(n,{position:"absolute"});
}
}
this._widget=w;
}
return this._widget;
},_getWidgetWidth:function(){
var p=this._params;
if(p.width){
return p.width;
}
var w=this._getWidget();
if(w){
return _2.get(w.domNode,"width");
}
return 10;
},_getWidgetHeight:function(){
var p=this._params;
if(p.height){
return p.height;
}
var w=this._getWidget();
if(w){
return _2.get(w.domNode,"height");
}
return 10;
},render:function(){
var _9=this.getLayer();
var _a=this._getWidget();
if(_a==null){
return;
}
var _b=this._params;
var _c=_b.longitude;
var _d=_b.latitude;
var _e=this.getCoordinateSystem();
var _f=_9.getDojoMap();
var p=_f.transformXY(_c,_d,_e);
var a=this._getLocalXY(p);
var _10=this._getWidgetWidth();
var _11=this._getWidgetHeight();
var x=a[0]-_10/2;
var y=a[1]-_11/2;
var dom=_a.domNode;
var pa=_9.olLayer.div;
if(dom.parentNode!=pa){
if(dom.parentNode){
dom.parentNode.removeChild(dom);
}
pa.appendChild(dom);
}
this._updateWidgetPosition({x:x,y:y,width:_10,height:_11});
},_updateWidgetPosition:function(box){
var w=this._widget;
var dom=w.domNode;
_2.set(dom,{position:"absolute",left:box.x+"px",top:box.y+"px",width:box.width+"px",height:box.height+"px"});
if(w.srcNodeRef){
_2.set(w.srcNodeRef,{position:"absolute",left:box.x+"px",top:box.y+"px",width:box.width+"px",height:box.height+"px"});
}
if(_3.isFunction(w.resize)){
w.resize({w:box.width,h:box.height});
}
},remove:function(){
var w=this.getWidget();
if(!w){
return;
}
var dom=w.domNode;
if(dom.parentNode){
dom.parentNode.removeChild(dom);
}
}});
});
