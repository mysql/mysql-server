//>>built
define("dojox/dgauges/RectangularValueIndicator",["dojo/_base/declare","./ScaleIndicatorBase","dojox/gfx","dojo/_base/event","dojo/dom-geometry"],function(_1,_2,_3,_4,_5){
return _1("dojox.dgauges.RectangularValueIndicator",_2,{paddingLeft:0,paddingTop:0,paddingRight:0,paddingBottom:0,constructor:function(){
this.addInvalidatingProperties(["paddingTop","paddingLeft","paddingRight","paddingBottom"]);
},indicatorShapeFunc:function(_6,_7){
return _6.createPolyline([0,0,10,0,0,10,-10,0,0,0]).setStroke({color:"black",width:1});
},refreshRendering:function(){
this.inherited(arguments);
var v=isNaN(this._transitionValue)?this.value:this._transitionValue;
var _8=this.scale.positionForValue(v);
var dx=0,dy=0;
var _9=0;
if(this.scale._gauge.orientation=="horizontal"){
dx=_8;
dy=this.paddingTop;
}else{
dx=this.paddingLeft;
dy=_8;
_9=90;
}
this._gfxGroup.setTransform([{dx:dx,dy:dy},_3.matrix.rotateg(_9)]);
},_onMouseDown:function(_a){
this.inherited(arguments);
var np=_5.position(this.scale._gauge.domNode,true);
this.set("value",this.scale.valueForPosition({x:_a.pageX-np.x,y:_a.pageY-np.y}));
_4.stop(_a);
},_onMouseMove:function(_b){
this.inherited(arguments);
var np=_5.position(this.scale._gauge.domNode,true);
this.set("value",this.scale.valueForPosition({x:_b.pageX-np.x,y:_b.pageY-np.y}));
}});
});
