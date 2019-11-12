//>>built
define("dojox/dgauges/CircularValueIndicator",["dojo/_base/declare","dojox/gfx","./ScaleIndicatorBase","dojo/_base/event"],function(_1,_2,_3,_4){
return _1("dojox.dgauges.CircularValueIndicator",_3,{indicatorShapeFunc:function(_5,_6){
return _5.createLine({x1:0,y1:0,x2:40,y2:0}).setStroke({color:"black",width:1});
},refreshRendering:function(){
this.inherited(arguments);
var v=isNaN(this._transitionValue)?this.value:this._transitionValue;
var _7=this.scale.positionForValue(v);
this._gfxGroup.setTransform([{dx:this.scale.originX,dy:this.scale.originY},_2.matrix.rotateg(_7)]);
},_onMouseDown:function(_8){
this.inherited(arguments);
var _9=this.scale._gauge._gaugeToPage(this.scale.originX,this.scale.originY);
var _a=((Math.atan2(_8.pageY-_9.y,_8.pageX-_9.x))*180)/(Math.PI);
this.set("value",this.scale.valueForPosition(_a));
_4.stop(_8);
},_onMouseMove:function(_b){
this.inherited(arguments);
var _c=this.scale._gauge._gaugeToPage(this.scale.originX,this.scale.originY);
var _d=((Math.atan2(_b.pageY-_c.y,_b.pageX-_c.x))*180)/(Math.PI);
this.set("value",this.scale.valueForPosition(_d));
}});
});
