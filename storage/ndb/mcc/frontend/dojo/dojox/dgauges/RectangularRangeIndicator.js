//>>built
define("dojox/dgauges/RectangularRangeIndicator",["dojo/_base/declare","dojox/gfx","./ScaleIndicatorBase","dojo/_base/event","dojo/dom-geometry"],function(_1,_2,_3,_4,_5){
return _1("dojox.dgauges.RectangularRangeIndicator",_3,{start:0,startThickness:10,endThickness:10,fill:null,stroke:null,paddingLeft:10,paddingTop:10,paddingRight:10,paddingBottom:10,constructor:function(){
this.fill=[255,120,0];
this.stroke={color:"black",width:0.2};
this.interactionMode="none";
this.addInvalidatingProperties(["start","startThickness","endThickness","fill","stroke"]);
},_defaultHorizontalShapeFunc:function(_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var gp=[_9,_a,_b,_a,_b,_a+_d,_9,_a+_c,_9,_a];
if(_e&&_e.colors){
_e.x1=_9;
_e.y1=_a;
_e.x2=_b;
_e.y2=_a;
}
return _7.createPolyline(gp).setFill(_e).setStroke(_f);
},_defaultVerticalShapeFunc:function(_10,_11,_12,_13,_14,_15,_16,_17,_18,_19){
var gp=[_13,_14,_13,_15,_13+_17,_15,_13+_16,_14,_13,_14];
if(_18&&_18.colors){
_18.x1=_13;
_18.y1=_14;
_18.x2=_13;
_18.y2=_15;
}
return _11.createPolyline(gp).setFill(_18).setStroke(_19);
},_shapeFunc:function(_1a,_1b,_1c,_1d,_1e,_1f,_20,_21,_22,_23){
if(this.scale._gauge.orientation=="horizontal"){
this._defaultHorizontalShapeFunc(_1a,_1b,_1c,_1d,_1e,_1f,_20,_21,_22,_23);
}else{
this._defaultVerticalShapeFunc(_1a,_1b,_1c,_1d,_1e,_1f,_20,_21,_22,_23);
}
},refreshRendering:function(){
this.inherited(arguments);
if(this._gfxGroup==null||this.scale==null){
return;
}
var _24=this.scale.positionForValue(this.start);
var v=isNaN(this._transitionValue)?this.value:this._transitionValue;
var pos=this.scale.positionForValue(v);
this._gfxGroup.clear();
var _25;
var _26;
var _27;
if(this.scale._gauge.orientation=="horizontal"){
_25=_24;
_26=this.paddingTop;
_27=pos;
}else{
_25=this.paddingLeft;
_26=_24;
_27=pos;
}
this._shapeFunc(this,this._gfxGroup,this.scale,_25,_26,_27,this.startThickness,this.endThickness,this.fill,this.stroke);
},_onMouseDown:function(_28){
this.inherited(arguments);
var np=_5.position(this.scale._gauge.domNode,true);
this.set("value",this.scale.valueForPosition({x:_28.pageX-np.x,y:_28.pageY-np.y}));
_4.stop(_28);
},_onMouseMove:function(_29){
this.inherited(arguments);
var np=_5.position(this.scale._gauge.domNode,true);
this.set("value",this.scale.valueForPosition({x:_29.pageX-np.x,y:_29.pageY-np.y}));
}});
});
