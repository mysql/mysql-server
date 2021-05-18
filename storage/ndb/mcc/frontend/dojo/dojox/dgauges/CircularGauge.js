//>>built
define("dojox/dgauges/CircularGauge",["dojo/_base/declare","dojo/dom-geometry","dojox/gfx","./GaugeBase"],function(_1,_2,_3,_4){
return _1("dojox.dgauges.CircularGauge",_4,{_transformProperties:null,refreshRendering:function(){
if(this._widgetBox.w<=0||this._widgetBox.h<=0){
return;
}
for(var _5 in this._elementsIndex){
this._elementsRenderers[_5]=this._elementsIndex[_5].refreshRendering();
}
var bb=this._computeBoundingBox(this._gfxGroup);
var _6=(bb.x+bb.width)/(bb.y+bb.height);
var _7=this._widgetBox.w;
var _8=this._widgetBox.h;
var _9=this._widgetBox.w/this._widgetBox.h;
var _a=0;
var _b=0;
var h=0;
var w=0;
if(_6>_9){
w=_7;
h=w/_6;
_b=(_8-h)/2;
}else{
h=_8;
w=h*_6;
_a=(_7-w)/2;
}
var _c=Math.max(w/(bb.x+bb.width),h/(bb.y+bb.height));
this._transformProperties={scale:_c,tx:_a,ty:_b};
this._gfxGroup.setTransform([_3.matrix.scale(_c),_3.matrix.translate(_a/_c,_b/_c)]);
},_gaugeToPage:function(px,py){
if(this._transformProperties){
var np=_2.position(this.domNode,true);
return {x:np.x+px*this._transformProperties.scale+this._transformProperties.tx,y:np.y+py*this._transformProperties.scale+this._transformProperties.ty};
}else{
return null;
}
}});
});
