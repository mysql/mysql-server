//>>built
define("dojox/geo/charting/Map",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/html","dojo/dom","dojo/dom-geometry","dojo/dom-class","dojo/_base/xhr","dojo/_base/connect","dojo/_base/window","dojox/gfx","dojox/geo/charting/_base","dojox/geo/charting/Feature","dojox/geo/charting/_Marker","dojo/number","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,has){
return _3("dojox.geo.charting.Map",null,{defaultColor:"#B7B7B7",highlightColor:"#D5D5D5",series:[],dataBindingAttribute:null,dataBindingValueFunction:null,dataStore:null,showTooltips:true,enableFeatureZoom:true,colorAnimationDuration:0,_idAttributes:null,_onSetListener:null,_onNewListener:null,_onDeleteListener:null,constructor:function(_10,_11){
_4.style(_10,"display","block");
this.container=_10;
var _12=this._getContainerBounds();
this.surface=_b.createSurface(_10,_12.w,_12.h);
this._createZoomingCursor();
this.mapBackground=this.surface.createRect({x:0,y:0,width:_12.w,height:_12.w}).setFill("rgba(0,0,0,0)");
this.mapObj=this.surface.createGroup();
this.mapObj.features={};
if(typeof _11=="object"){
this._init(_11);
}else{
if(typeof _11=="string"&&_11.length>0){
_8.get({url:_11,handleAs:"json",sync:true,load:_1.hitch(this,"_init")});
}
}
},_getContainerBounds:function(){
var _13=_6.position(this.container,true);
var _14=_6.getMarginBox(this.container);
var _15=_6.getContentBox(this.container);
this._storedContainerBounds={x:_13.x,y:_13.y,w:_15.w||100,h:_15.h||100};
return this._storedContainerBounds;
},resize:function(_16,_17,_18){
var _19=this._storedContainerBounds;
var _1a=this._getContainerBounds();
if((_19.w==_1a.w)&&(_19.h==_1a.h)){
return;
}
this.mapBackground.setShape({width:_1a.w,height:_1a.h});
this.surface.setDimensions(_1a.w,_1a.h);
this.mapObj.marker.hide();
this.mapObj.marker._needTooltipRefresh=true;
if(_16){
var _1b=this.getMapScale();
var _1c=_1b;
if(_17){
var _1d=this.mapObj.boundBox;
var _1e=_1a.w/_19.w;
var _1f=_1a.h/_19.h;
_1c=_1b*Math.sqrt(_1e*_1f);
}
var _20=this.screenCoordsToMapCoords(_19.w/2,_19.h/2);
this.setMapCenterAndScale(_20.x,_20.y,_1c,_18);
}
},_isMobileDevice:function(){
return (has("safari")&&(navigator.userAgent.indexOf("iPhone")>-1||navigator.userAgent.indexOf("iPod")>-1||navigator.userAgent.indexOf("iPad")>-1))||(navigator.userAgent.toLowerCase().indexOf("android")>-1);
},setMarkerData:function(_21){
_8.get({url:_21,handleAs:"json",handle:_1.hitch(this,"_appendMarker")});
},setDataBindingAttribute:function(_22){
this.dataBindingAttribute=_22;
if(this.dataStore){
this._queryDataStore();
}
},setDataBindingValueFunction:function(_23){
this.dataBindingValueFunction=_23;
if(this.dataStore){
this._queryDataStore();
}
},_queryDataStore:function(){
if(!this.dataBindingAttribute||(this.dataBindingAttribute.length==0)){
return;
}
var _24=this;
this.dataStore.fetch({scope:this,onComplete:function(_25){
this._idAttributes=_24.dataStore.getIdentityAttributes({});
_2.forEach(_25,function(_26){
var id=_24.dataStore.getValue(_26,this._idAttributes[0]);
if(_24.mapObj.features[id]){
var val=null;
var _27=_24.dataStore.getValue(_26,_24.dataBindingAttribute);
if(_27){
if(this.dataBindingValueFunction){
val=this.dataBindingValueFunction(_27);
}else{
if(isNaN(val)){
val=_f.parse(_27);
}else{
val=_27;
}
}
}
if(val){
_24.mapObj.features[id].setValue(val);
}
}
},this);
}});
},_onSet:function(_28,_29,_2a,_2b){
var id=this.dataStore.getValue(_28,this._idAttributes[0]);
var _2c=this.mapObj.features[id];
if(_2c&&(_29==this.dataBindingAttribute)){
if(_2b){
_2c.setValue(_2b);
}else{
_2c.unsetValue();
}
}
},_onNew:function(_2d,_2e){
var id=this.dataStore.getValue(item,this._idAttributes[0]);
var _2f=this.mapObj.features[id];
if(_2f&&(attribute==this.dataBindingAttribute)){
_2f.setValue(newValue);
}
},_onDelete:function(_30){
var id=_30[this._idAttributes[0]];
var _31=this.mapObj.features[id];
if(_31){
_31.unsetValue();
}
},setDataStore:function(_32,_33){
if(this.dataStore!=_32){
if(this._onSetListener){
_9.disconnect(this._onSetListener);
_9.disconnect(this._onNewListener);
_9.disconnect(this._onDeleteListener);
}
this.dataStore=_32;
if(_32){
_onSetListener=_9.connect(this.dataStore,"onSet",this,this._onSet);
_onNewListener=_9.connect(this.dataStore,"onNew",this,this._onNew);
_onDeleteListener=_9.connect(this.dataStore,"onDelete",this,this._onDelete);
}
}
if(_33){
this.setDataBindingAttribute(_33);
}
},addSeries:function(_34){
if(typeof _34=="object"){
this._addSeriesImpl(_34);
}else{
if(typeof _34=="string"&&_34.length>0){
_8.get({url:_34,handleAs:"json",sync:true,load:_1.hitch(this,function(_35){
this._addSeriesImpl(_35.series);
})});
}
}
},_addSeriesImpl:function(_36){
this.series=_36;
for(var _37 in this.mapObj.features){
var _38=this.mapObj.features[_37];
_38.setValue(_38.value);
}
},fitToMapArea:function(_39,_3a,_3b,_3c){
if(!_3a){
_3a=0;
}
var _3d=_39.w,_3e=_39.h,_3f=this._getContainerBounds(),_40=Math.min((_3f.w-2*_3a)/_3d,(_3f.h-2*_3a)/_3e);
this.setMapCenterAndScale(_39.x+_39.w/2,_39.y+_39.h/2,_40,_3b,_3c);
},fitToMapContents:function(_41,_42,_43){
var _44=this.mapObj.boundBox;
this.fitToMapArea(_44,_41,_42,_43);
},setMapCenter:function(_45,_46,_47,_48){
var _49=this.getMapScale();
this.setMapCenterAndScale(_45,_46,_49,_47,_48);
},_createAnimation:function(_4a,_4b,_4c,_4d){
var _4e=_4b.dx?_4b.dx:0;
var _4f=_4b.dy?_4b.dy:0;
var _50=_4c.dx?_4c.dx:0;
var _51=_4c.dy?_4c.dy:0;
var _52=_4b.xx?_4b.xx:1;
var _53=_4c.xx?_4c.xx:1;
var _54=_b.fx.animateTransform({duration:1000,shape:_4a,transform:[{name:"translate",start:[_4e,_4f],end:[_50,_51]},{name:"scale",start:[_52],end:[_53]}]});
if(_4d){
var _55=_9.connect(_54,"onEnd",this,function(_56){
_4d(_56);
_9.disconnect(_55);
});
}
return _54;
},setMapCenterAndScale:function(_57,_58,_59,_5a,_5b){
var _5c=this.mapObj.boundBox;
var _5d=this._getContainerBounds();
var _5e=_5d.w/2-_59*(_57-_5c.x);
var _5f=_5d.h/2-_59*(_58-_5c.y);
var _60=new _b.matrix.Matrix2D({xx:_59,yy:_59,dx:_5e,dy:_5f});
var _61=this.mapObj.getTransform();
if(!_5a||!_61){
this.mapObj.setTransform(_60);
}else{
var _62=this._createAnimation(this.mapObj,_61,_60,_5b);
_62.play();
}
},getMapCenter:function(){
var _63=this._getContainerBounds();
return this.screenCoordsToMapCoords(_63.w/2,_63.h/2);
},setMapScale:function(_64,_65,_66){
var _67=this._getContainerBounds();
var _68=this.screenCoordsToMapCoords(_67.w/2,_67.h/2);
this.setMapScaleAt(_64,_68.x,_68.y,_65,_66);
},setMapScaleAt:function(_69,_6a,_6b,_6c,_6d){
var _6e=null;
var _6f=null;
_6e={x:_6a,y:_6b};
_6f=this.mapCoordsToScreenCoords(_6e.x,_6e.y);
var _70=this.mapObj.boundBox;
var _71=_6f.x-_69*(_6e.x-_70.x);
var _72=_6f.y-_69*(_6e.y-_70.y);
var _73=new _b.matrix.Matrix2D({xx:_69,yy:_69,dx:_71,dy:_72});
var _74=this.mapObj.getTransform();
if(!_6c||!_74){
this.mapObj.setTransform(_73);
}else{
var _75=this._createAnimation(this.mapObj,_74,_73,_6d);
_75.play();
}
},getMapScale:function(){
var mat=this.mapObj.getTransform();
var _76=mat?mat.xx:1;
return _76;
},mapCoordsToScreenCoords:function(_77,_78){
var _79=this.mapObj.getTransform();
var _7a=_b.matrix.multiplyPoint(_79,_77,_78);
return _7a;
},screenCoordsToMapCoords:function(_7b,_7c){
var _7d=_b.matrix.invert(this.mapObj.getTransform());
var _7e=_b.matrix.multiplyPoint(_7d,_7b,_7c);
return _7e;
},deselectAll:function(){
for(var _7f in this.mapObj.features){
this.mapObj.features[_7f].select(false);
}
this.selectedFeature=null;
this.focused=false;
},_init:function(_80){
this.mapObj.boundBox={x:_80.layerExtent[0],y:_80.layerExtent[1],w:(_80.layerExtent[2]-_80.layerExtent[0]),h:_80.layerExtent[3]-_80.layerExtent[1]};
this.fitToMapContents(3);
_2.forEach(_80.featureNames,function(_81){
var _82=_80.features[_81];
_82.bbox.x=_82.bbox[0];
_82.bbox.y=_82.bbox[1];
_82.bbox.w=_82.bbox[2];
_82.bbox.h=_82.bbox[3];
var _83=new _d(this,_81,_82);
_83.init();
this.mapObj.features[_81]=_83;
},this);
this.mapObj.marker=new _e({},this);
},_appendMarker:function(_84){
this.mapObj.marker=new _e(_84,this);
},_createZoomingCursor:function(){
if(!_5.byId("mapZoomCursor")){
var _85=_a.doc.createElement("div");
_4.attr(_85,"id","mapZoomCursor");
_7.add(_85,"mapZoomIn");
_4.style(_85,"display","none");
_a.body().appendChild(_85);
}
},onFeatureClick:function(_86){
},onFeatureOver:function(_87){
},onZoomEnd:function(_88){
}});
});
